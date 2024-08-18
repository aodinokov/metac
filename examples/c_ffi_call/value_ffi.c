#include "value_ffi.h"

#include "metac/backend/helpers.h"
#include "metac/endian.h"

#include <assert.h>
#include <errno.h>
#include <ffi.h>
#include <stdlib.h> /*calloc, free*/
#include <string.h>

#include "metac/backend/va_list_ex.h"

/**
 * \page ffi_call FFI call example
 * 
 * some thoughts on ffi.
 * 
 * it seems that there are some different ways to set the size of structures.
 * the [documentation](https://github.com/libffi/libffi/blob/master/doc/libffi.texi#L509) suggests
 * setting both struct size and alignment to zero and to set elements sizes.
 * 
 * this approach worked well until we started working with aligned struct members and aligned
 * structures. we're using members which are standard types. and it seems like because of that
 * ffi couldn't calculate the proper size of structs/arrays.
 * 
 * we ended up with setting struct/array size. we still set elements, but
 * most likely for aligned cases their offset won't be correct.
 * right now all tests with aligned structures of data pass ok, even without
 * setting the correct offset.
 * 
 * Anyway, [here is](https://github.com/libffi/libffi/blob/8e3ef965c2d0015ed129a06d0f11f30c2120a413/testsuite/libffi.call/va_struct1.c)
 * and example of how to check the offset after alignment is set.
 * For now we see couple of TODO in the code below, but since everything works ok the code is kept as-is.
 * may be in future we can make it ideal.
 * 
*/


struct _va_list_entry {
    struct va_list_container va_list_c;
    metac_value_t * p_val;
    metac_num_t id;
};

// convert function parameters types to ffi_type (metac->ffi)
static int _val_to_ffi_type(metac_entry_t * p_entry, metac_flag_t variadic, ffi_type ** pp_rtype) {

    if (metac_entry_is_base_type(p_entry) != 0) {

#define _process_(_metac_type_, _ffi_type_) \
        if (metac_entry_is_##_metac_type_(p_entry)) { \
            ffi_type * p_ffi_type = calloc(1, sizeof(ffi_type)); \
            if (p_ffi_type == NULL) { \
                return -ENOMEM; \
            } \
            memcpy(p_ffi_type, &_ffi_type_, sizeof(ffi_type)); \
            *pp_rtype = p_ffi_type; \
            return 0; \
        } 

        _process_(char, ffi_type_schar);
        _process_(uchar, ffi_type_uchar);
        _process_(short, ffi_type_sshort);
        _process_(ushort, ffi_type_ushort);
        _process_(int, ffi_type_sint);
        _process_(uint, ffi_type_uint);
        _process_(long, ffi_type_slong);
        _process_(ulong, ffi_type_ulong);
        _process_(llong, ffi_type_slong);
        _process_(ullong, ffi_type_ulong);
        _process_(bool, ffi_type_uchar);
        _process_(float, ffi_type_float);
        _process_(double, ffi_type_double);
        _process_(ldouble, ffi_type_longdouble);
        _process_(float_complex, ffi_type_complex_float);
        _process_(double_complex, ffi_type_complex_double);
        _process_(ldouble_complex, ffi_type_complex_longdouble);
#undef _process_
    } else if (metac_entry_is_enumeration(p_entry) != 0) {
        metac_size_t param_byte_sz = 0;
        if (metac_entry_byte_size(p_entry, &param_byte_sz) != 0) {
            return -(EFAULT);
        }

#define _process_(_type_size_, _ffi_type_) \
        if (param_byte_sz == sizeof(_type_size_)) { \
            ffi_type * p_ffi_type = calloc(1, sizeof(ffi_type)); \
            if (p_ffi_type == NULL) { \
                return -ENOMEM; \
            } \
            memcpy(p_ffi_type, &_ffi_type_, sizeof(ffi_type)); \
            *pp_rtype = p_ffi_type; \
            return 0; \
        } 

        _process_(char, ffi_type_schar);
        _process_(short, ffi_type_sshort);
        _process_(int, ffi_type_sint);
        _process_(long, ffi_type_slong);
        _process_(long long, ffi_type_slong);
#undef _process_
    } else if (
        metac_entry_is_va_list_parameter(p_entry) != 0 ) {
        ffi_type * p_ffi_type = calloc(1, sizeof(ffi_type));
        if (p_ffi_type == NULL) {
            return -ENOMEM;
        }
#if __linux__
        va_list x;
        p_ffi_type->type = FFI_TYPE_STRUCT;
        p_ffi_type->size = sizeof(void*);
        p_ffi_type->alignment = p_ffi_type->size;
        p_ffi_type->elements = calloc(p_ffi_type->size/sizeof(void*) + 1 /* for NULL */, sizeof(ffi_type *));
        if (p_ffi_type->elements == NULL) {
            free(p_ffi_type);
            return -(ENOMEM);
        }
        for (int i = 0; i < p_ffi_type->size/sizeof(void*); ++i) {
            p_ffi_type->elements[i] = calloc(1, sizeof(ffi_type));
            if (p_ffi_type->elements[i] != NULL) {
                memcpy(p_ffi_type->elements[i], &ffi_type_pointer, sizeof(ffi_type));
            }
        }
        //memcpy(p_ffi_type, &ffi_type_pointer, sizeof(ffi_type));
#else
        memcpy(p_ffi_type, &ffi_type_pointer, sizeof(ffi_type));
#endif
        *pp_rtype = p_ffi_type;
        return 0;
    } else if (
        metac_entry_is_pointer(p_entry) != 0) {
        ffi_type * p_ffi_type = calloc(1, sizeof(ffi_type));
        if (p_ffi_type == NULL) {
            return -ENOMEM;
        }
        memcpy(p_ffi_type, &ffi_type_pointer, sizeof(ffi_type));
        *pp_rtype = p_ffi_type;
        return 0;
    } else if (
            variadic != 0 && (
            metac_entry_has_members(p_entry) != 0 ||
            metac_entry_has_elements(p_entry) != 0)) {
        ffi_type * p_ffi_type = calloc(1, sizeof(ffi_type));
        if (p_ffi_type == NULL) {
            return -ENOMEM;
        }
        memcpy(p_ffi_type, &ffi_type_pointer, sizeof(ffi_type));
        *pp_rtype = p_ffi_type;
        return 0;        
    } else if (metac_entry_has_members(p_entry) != 0) {
        // https://github.com/libffi/libffi/blob/master/doc/libffi.texi#L622
        metac_num_t memb_count = metac_entry_member_count(p_entry);

        // if p_entry is union - select largest element and skip others
        if (metac_entry_kind(metac_entry_final_entry(p_entry, NULL)) == METAC_KND_union_type && memb_count > 0) {
            metac_entry_t * p_largest_entry = NULL;
            metac_size_t largest_sz = 0, sz = 0;
            for (metac_num_t i = 0; i < memb_count; ++i) {
                metac_entry_t * p_memb_entry = metac_entry_by_member_id(p_entry, i);
                if (p_memb_entry == NULL) {
                    return -(EFAULT);
                }
                if (metac_entry_byte_size(p_memb_entry, &sz) != 0) {
                    return -(EFAULT);
                }
                if (p_largest_entry == NULL || sz > largest_sz) { 
                    p_largest_entry = p_memb_entry;
                    largest_sz = sz;
                }
            }
            assert(p_largest_entry != NULL);
            return _val_to_ffi_type(p_largest_entry, variadic, pp_rtype);
        }

        metac_size_t e_sz = 0;
        if (metac_entry_byte_size(p_entry, &e_sz) != 0) {
            return -(EFAULT);
        }

        ffi_type * p_tmp = calloc(1, sizeof(ffi_type));
        if (p_tmp == NULL) {
            return -(ENOMEM);
        }
        p_tmp->elements = calloc(memb_count + 1 /* for NULL */, sizeof(ffi_type *));
        if (p_tmp->elements == NULL) {
            free(p_tmp);
            return -(ENOMEM);
        }
        p_tmp->size = e_sz;     // it appeared that this is enough to make struct work. TODO: alginment of memebers
        p_tmp->alignment = 0;
        p_tmp->type = FFI_TYPE_STRUCT;

        metac_num_t memb_id = 0; // this is actual id, we're ignoring fields with the same offset
        //metac_offset_t current_offset, last_offset = 0;
        metac_flag_t bitfield_state = 0;
        metac_offset_t bitfield_start_byte_offset = 0;
        for (metac_num_t i = 0; i < memb_count; ++i) {

            metac_entry_t *p_memb_entry = metac_entry_by_member_id(p_entry, i);
            if (p_memb_entry == NULL) {
                free(p_tmp->elements);
                free(p_tmp);
                return -(EFAULT);
            }

            // handle bitfields - ignore multiple entries by the same byte offset (they all will have the same type)
            struct metac_member_raw_location_info bitfields_raw_info;
            if (metac_entry_member_raw_location_info(p_memb_entry, &bitfields_raw_info) != 0) {
                free(p_tmp->elements);
                free(p_tmp);
                return -(EFAULT);
            }
            if (bitfields_raw_info.p_bit_offset == NULL && /* member of the struct. addr points to the beginning of the struct */
                bitfields_raw_info.p_bit_size == NULL &&
                bitfields_raw_info.p_data_bit_offset == NULL) { /* not a bit field */

                if (bitfield_state != 0) {
                    metac_offset_t delta = bitfields_raw_info.byte_offset - bitfield_start_byte_offset;
                    while(delta > 0) {
                        // simulate fields with appropriate sizes - their number will be less or equal than number of bitfield
#define _process_(_type_size_, _ffi_type_) \
                        if (delta >= sizeof(_type_size_)) { \
                            /*printf("delta %d simulating %s\n", (int)delta, #_type_size_);*/ \
                            p_tmp->elements[memb_id] = calloc(1, sizeof(ffi_type)); \
                            if (p_tmp->elements[memb_id] == NULL) { \
                                free(p_tmp->elements); \
                                free(p_tmp); \
                                return -ENOMEM; \
                            } \
                            memcpy(p_tmp->elements[memb_id], &_ffi_type_, sizeof(ffi_type)); \
                            delta -= sizeof(_type_size_); \
                            ++memb_id; \
                            if (memb_id == memb_count+1) { \
                                /* this must not happen */ \
                                free(p_tmp->elements); \
                                free(p_tmp); \
                                return -(EFAULT); \
                            } \
                            continue; \
                        }
                        _process_(long long, ffi_type_slong);
                        _process_(long, ffi_type_slong);
                        _process_(int, ffi_type_sint);
                        _process_(short, ffi_type_sshort);
                        _process_(char, ffi_type_schar);
                    }
                }
                bitfield_state = 0;

                if (_val_to_ffi_type(p_memb_entry, variadic, &p_tmp->elements[memb_id]) != 0) {
                    free(p_tmp->elements); // TODO: cleanup _tmp->elements prior to
                    free(p_tmp);
                    return -(EFAULT);
                }
            } else { // bitfields
                metac_offset_t byte_offset = 0, bit_offset = 0, bit_size = 0;
                if (metac_entry_member_bitfield_offsets(p_memb_entry, &byte_offset, &bit_offset, &bit_size) != 0) {
                    free(p_tmp->elements);
                    free(p_tmp);
                    return -(EFAULT);
                }
                if (bitfield_state == 0) {
                    bitfield_state = 1;
                    bitfield_start_byte_offset = byte_offset;
                }
                continue;
            }
            ++memb_id;
        }

        if (bitfield_state != 0) {
            metac_offset_t delta = e_sz - bitfield_start_byte_offset;
            while(delta > 0) {
                // simulate fields with appropriate sizes - their number will be less or equal than number of bitfield
                _process_(long long, ffi_type_slong);
                _process_(long, ffi_type_slong);
                _process_(int, ffi_type_sint);
                _process_(short, ffi_type_sshort);
                _process_(char, ffi_type_schar);
#undef _process_
            }
        }

        p_tmp->elements[memb_id] = NULL;
        *pp_rtype = p_tmp;
        return 0;
    } else if (metac_entry_has_elements(p_entry) != 0) {
        // https://github.com/libffi/libffi/blob/master/doc/libffi.texi#L509
        metac_num_t el_count = metac_entry_element_count(p_entry);
        if (el_count < 0) { // flexible, we have to consider as 0 len
            el_count = 0;
        }
        metac_entry_t * p_el_entry = metac_entry_element_entry(p_entry);
        if (p_el_entry == NULL) {
            return -(EFAULT);
        }

        metac_size_t e_sz = 0;
        if (el_count != 0 && metac_entry_byte_size(p_entry, &e_sz) != 0) {
            return -(EFAULT);
        }

        ffi_type * p_tmp = calloc(1, sizeof(ffi_type));
        if (p_tmp == NULL) {
            return -(ENOMEM);
        }
        p_tmp->elements = calloc(el_count + 1 /* for NULL */, sizeof(ffi_type *));
        if (p_tmp->elements == NULL) {
            free(p_tmp);
            return -(ENOMEM);
        }
        p_tmp->size = e_sz; // it appeared that this is enough to make struct work. TODO: alginment of memebers
        p_tmp->alignment = 0;
        p_tmp->type = FFI_TYPE_STRUCT;

        metac_num_t el_id = 0;
        for (metac_num_t i = 0; i < el_count; ++i) {
            if (_val_to_ffi_type(p_el_entry, variadic, &p_tmp->elements[el_id]) != 0) {
                free(p_tmp->elements);
                free(p_tmp);
                return -(EFAULT);
            }
            ++el_id;
        }
        p_tmp->elements[el_id] = NULL;
        *pp_rtype = p_tmp;
        return 0;
    } else {
         return -(ENOTSUP);
    }
    return 0;
}

void _cleanup_ffi_type(ffi_type * p_rtype) {
    if (p_rtype == NULL) {
        return;
    }
    if (p_rtype->type == FFI_TYPE_STRUCT && p_rtype->elements != NULL) {
        metac_num_t i = 0;
        while (p_rtype->elements[i] != NULL) {
            _cleanup_ffi_type(p_rtype->elements[i]);
            ++i;
        }
        free(p_rtype->elements);
        p_rtype->elements = NULL;
    }
    free(p_rtype);
}

int _call(metac_value_t * p_param_storage_val, void (*fn)(void), metac_value_t * p_res_value,
    struct _va_list_entry * p_val_list_entries, metac_num_t va_list_number) {
    // to support va_list
    struct va_list_container va_list_c;
    void * _ptr_ = NULL;

    _check_(
        p_param_storage_val == NULL ||
        metac_value_has_parameter_load(p_param_storage_val) == 0 ||
        fn == NULL, -(EINVAL));

    metac_entry_t *p_param_storage_entry = metac_value_entry(p_param_storage_val);
    _check_(p_param_storage_entry == NULL, -(EFAULT));
    _check_(metac_entry_has_parameters(p_param_storage_entry) == 0, -(EINVAL));

    ffi_cif cif;
    ffi_type * rtype = NULL;
    ffi_arg rc;
    metac_size_t res_sz = 0;
    void * res_addr = NULL;

    if (metac_entry_has_result(p_param_storage_entry) != 0) {
        if (p_res_value == NULL) {
            return -(EINVAL);
        }

        int fill_in_res = _val_to_ffi_type(metac_value_entry(p_res_value), 0, &rtype);
        if (fill_in_res != 0) {
            return -(EFAULT);
        }
    }

    metac_num_t param_count  = metac_value_parameter_count(p_param_storage_val);
    metac_num_t variadic_param_count = 0;
    metac_value_t * p_last_param_val = NULL;

    if (metac_entry_parameters_count(p_param_storage_entry) < param_count - 1) {
        // something wrong with storage (maybe it wasn't filled)
        return -(EFAULT);
    }

    if (param_count > 0) { // check the last param if it's variadic
        metac_entry_t *p_last_member = metac_entry_by_paremeter_id(p_param_storage_entry, param_count - 1);
        if (p_last_member == NULL) {
            return -(EFAULT);
        }

        if (metac_entry_is_unspecified_parameter(p_last_member) != 0) {
            p_last_param_val = metac_value_parameter_new_item(p_param_storage_val, param_count - 1);
            if (p_last_param_val == NULL) { /* something went wrong */
                return -(EFAULT);
            }

            --param_count; // because the last param is to keep all variadica params
            variadic_param_count = metac_value_parameter_count(p_last_param_val);
        }
    }

    // args
    ffi_type **args = NULL;
    // vals
    void **values = NULL;
    void **pvalues = NULL; // need to case of variadic string. we have array and this is to convert to pointer

    if (param_count + variadic_param_count > 0) {
        args = calloc(param_count + variadic_param_count, sizeof(ffi_type *));
        if (args == NULL) {
            if (p_last_param_val != NULL) { metac_value_delete(p_last_param_val); }
            return -(EFAULT);
        }
        values = calloc(param_count + variadic_param_count, sizeof(void *));
        if (values == NULL) {
            free(args);
            if (p_last_param_val != NULL) { metac_value_delete(p_last_param_val); }
            return -(EFAULT);
        }
        pvalues = calloc(param_count + variadic_param_count, sizeof(void *));
        if (values == NULL) {
            free(values);
            free(args);
            if (p_last_param_val != NULL) { metac_value_delete(p_last_param_val); }
            return -(EFAULT);
        }
    }

    // fill in non-variadic first
    metac_num_t va_list_number_cur = 0;
    for (metac_num_t i = 0 ; i < param_count; ++i) {
        metac_value_t * p_param_val = metac_value_parameter_new_item(p_param_storage_val, i);
        if (p_param_val == NULL) {
            free(pvalues);
            free(values);
            for (metac_num_t ic = 0; ic <= i; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            if (p_last_param_val != NULL) { metac_value_delete(p_last_param_val); }
            return -(EFAULT);
        }
        // fill in type
        int fill_in_res = _val_to_ffi_type(metac_value_entry(p_param_val), 0, &args[i]);
        if (fill_in_res != 0) {
            metac_value_delete(p_param_val);
            free(pvalues);
            free(values);
            for (metac_num_t ic = 0; ic <= i; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            if (p_last_param_val != NULL) { metac_value_delete(p_last_param_val); }
            return fill_in_res;
        }

        // fill in data
        if (metac_entry_is_va_list_parameter(metac_value_entry(p_param_val)) == 0) {
            values[i] = metac_value_addr(p_param_val);
        } else {
            assert(va_list_number_cur < va_list_number);
            assert(p_val_list_entries[va_list_number_cur].id == i);
#if __linux__
            // void **p1 = &p_val_list_entries[va_list_number_cur].va_list_c;
            // fprintf(stderr, "dbg:p1 %p: %p, %p p2\n", p1, *p1, *(p1+1));
            pvalues[i] = &(p_val_list_entries[va_list_number_cur].va_list_c);
            values[i] = &pvalues[i];
            // va_list cp;
            // va_copy(cp, p_val_list_entries[va_list_number_cur].va_list_c.parameters);
            // vfprintf(stderr, "dbg0: %x %x %x %x %x %x\n", cp);
            // va_end(cp);
            // //va_list cp;
            // va_copy(cp, p_val_list_entries[va_list_number_cur].va_list_c.parameters);
            // vfprintf(stderr, "dbg1: %x %x %x %x %x %x\n", cp);
            // va_end(cp);
#else
            values[i] = &p_val_list_entries[va_list_number_cur].va_list_c.parameters;
#endif
            ++va_list_number_cur;
        }
        metac_value_delete(p_param_val);
    }

    // fill in variadic if any
    for (metac_num_t i = param_count ; i < param_count + variadic_param_count; ++i) {
        assert(p_last_param_val != NULL);
        metac_value_t * p_param_val = metac_value_parameter_new_item(p_last_param_val, i - param_count);
        if (p_param_val == NULL) {
            free(pvalues);
            free(values);
            for (metac_num_t ic = 0; ic <= i; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            if (p_last_param_val != NULL) { metac_value_delete(p_last_param_val); }
            return -(EFAULT);
        }
        // fill in type
        int fill_in_res = _val_to_ffi_type(metac_value_entry(p_param_val), 1, &args[i]);
        if (fill_in_res != 0) {
            metac_value_delete(p_param_val);
            free(pvalues);
            free(values);
            for (metac_num_t ic = 0; ic <= i; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            if (p_last_param_val != NULL) { metac_value_delete(p_last_param_val); }
            return fill_in_res;
        }

        // fill in data
        if (metac_entry_has_members(metac_value_entry(p_param_val)) != 0 ||
            metac_entry_has_elements(metac_value_entry(p_param_val)) != 0) {
            pvalues[i] = metac_value_addr(p_param_val);
            values[i] = &pvalues[i];
        } else {
            values[i] = metac_value_addr(p_param_val);
        }
        metac_value_delete(p_param_val);
    }

    // set result arg
    if (metac_entry_has_result(p_param_storage_entry) != 0) {
        res_addr = &rc;
        if (metac_entry_byte_size(metac_value_entry(p_res_value), &res_sz) != 0) {
            free(pvalues);
            free(values);
            for (metac_num_t ic = 0; ic < param_count + variadic_param_count; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            return -(EFAULT);
        }
        if (res_sz >= sizeof(rc)) {
            res_addr = metac_value_addr(p_res_value);
            assert(res_addr != NULL);
        }
    }

    // ffi handles variadic and non-variadic in different way
    if (p_last_param_val == NULL) { // non variadic fn
        assert(variadic_param_count == 0);

        if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, param_count, rtype, args) != FFI_OK) {
            free(pvalues);
            free(values);
            for (metac_num_t ic = 0; ic < param_count + variadic_param_count; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            return -(EFAULT);
        }
    } else { // variadic fn
        metac_value_delete(p_last_param_val);
        p_last_param_val = NULL;

        if (ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, param_count, param_count + variadic_param_count, rtype, args) != FFI_OK) {
            free(pvalues);
            free(values);
            for (metac_num_t ic = 0; ic < param_count + variadic_param_count; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            return -(EFAULT);
        }
    }

    ffi_call(&cif, fn, res_addr, values);

    free(pvalues);
    free(values);
    for (metac_num_t ic = 0; ic < param_count + variadic_param_count; ++ic ) {_cleanup_ffi_type(args[ic]);}
    free(args);

    _cleanup_ffi_type(rtype);
    if (res_addr == &rc) {
        assert(metac_entry_has_result(p_param_storage_entry) != 0);
        // we need to pass result back to p_res_value if we used rc as buf (for small data)
        uint8_t * p_res_buf = metac_value_addr(p_res_value);
        for (metac_size_t i = 0;  i < res_sz; ++i) {
            if (IS_LITTLE_ENDIAN) {
                p_res_buf[i] = rc & 0xff;
            } else {
                p_res_buf[res_sz - i - 1] = rc & 0xff;
            }
            rc >>= 8;
        }
    }
    return 0;
}

static int _call_wrapper(metac_value_t * p_param_storage_val, void (*fn)(void), metac_value_t * p_res_value,
    metac_num_t va_list_number_cur, struct _va_list_entry * p_val_list_entries, metac_num_t va_list_number);

static int _call_wrapper_va(metac_value_t * p_param_storage_val, void (*fn)(void), metac_value_t * p_res_value,
    metac_num_t va_list_number_cur, struct _va_list_entry * p_val_list_entries, metac_num_t va_list_number, ...){
    int res;
    va_start(p_val_list_entries[va_list_number_cur].va_list_c.parameters, va_list_number);
    res = _call_wrapper(p_param_storage_val, fn, p_res_value,
            va_list_number_cur + 1, p_val_list_entries, va_list_number);
// #if __linux__
//             va_list cp;
//             va_copy(cp, p_val_list_entries[va_list_number_cur].va_list_c.parameters);
//             vfprintf(stderr, "dbg-after: %x %x %x %x %x %x\n", cp);
//             va_end(cp);
// #endif
    va_end(p_val_list_entries[va_list_number_cur].va_list_c.parameters);
    return res;
}

static int _call_wrapper(metac_value_t * p_param_storage_val, void (*fn)(void), metac_value_t * p_res_value,
    metac_num_t va_list_number_cur, struct _va_list_entry * p_val_list_entries, metac_num_t va_list_number) {
    
    if (va_list_number_cur < va_list_number) { // call _call_wrapper_va using ffi
        metac_value_t *p_va_list_val = p_val_list_entries[va_list_number_cur].p_val;
        assert(p_va_list_val != NULL);
        assert(metac_entry_is_va_list_parameter(metac_value_entry(p_va_list_val)) != 0);

        metac_num_t param_count = 6; // see _call_wrapper_va
        metac_num_t variadic_param_count = metac_value_parameter_count(p_va_list_val);

        ffi_cif cif;
        ffi_type * rtype = &ffi_type_sint;
        ffi_arg rc;

        ffi_type **args = NULL;
        void **values = NULL;
        void **pvalues = NULL; // need to case of variadic string. we have array and this is to convert to pointer


        if (param_count + variadic_param_count > 0) {
            args = calloc(param_count + variadic_param_count, sizeof(ffi_type *));
            if (args == NULL) {
                return -(EFAULT);
            }
            values = calloc(param_count + variadic_param_count, sizeof(void *));
            if (values == NULL) {
                free(args);
                return -(EFAULT);
            }
            pvalues = calloc(param_count + variadic_param_count, sizeof(void *));
            if (values == NULL) {
                free(values);
                free(args);
                return -(EFAULT);
            }
        }
        // fill in static
        args[0] = &ffi_type_pointer; values[0] = &p_param_storage_val;
        args[1] = &ffi_type_pointer; values[1] = &fn;
        args[2] = &ffi_type_pointer; values[2] = &p_res_value;
        args[3] = &ffi_type_sint; values[3] = &va_list_number_cur;
        args[4] = &ffi_type_pointer; values[4] = &p_val_list_entries;
        args[5] = &ffi_type_sint; values[5] = &va_list_number;

        // fill in variadic if any
        for (metac_num_t i = param_count ; i < param_count + variadic_param_count; ++i) {
            metac_value_t * p_param_val = metac_value_parameter_new_item(p_va_list_val, i - param_count);
            if (p_param_val == NULL) {
                free(pvalues);
                free(values);
                for (metac_num_t ic = param_count; ic <= i; ++ic ) {_cleanup_ffi_type(args[ic]);}
                free(args);
                return -(EFAULT);
            }
            // fill in type
            int fill_in_res = _val_to_ffi_type(metac_value_entry(p_param_val), 1, &args[i]);
            if (fill_in_res != 0) {
                metac_value_delete(p_param_val);
                free(pvalues);
                free(values);
                for (metac_num_t ic = param_count; ic <= i; ++ic ) {_cleanup_ffi_type(args[ic]);}
                free(args);
                return fill_in_res;
            }

            // fill in data
            if (metac_entry_has_members(metac_value_entry(p_param_val)) != 0 ||
                metac_entry_has_elements(metac_value_entry(p_param_val)) != 0) {
                pvalues[i] = metac_value_addr(p_param_val);
                values[i] = &pvalues[i];
            } else {
                values[i] = metac_value_addr(p_param_val);
            }
            metac_value_delete(p_param_val);
        }

        //call
        if (ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, param_count, param_count + variadic_param_count, rtype, args) != FFI_OK) {
            free(pvalues);
            free(values);
            for (metac_num_t ic = param_count; ic < param_count + variadic_param_count; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            return -(EFAULT);
        }
        ffi_call(&cif, (void (*)(void))_call_wrapper_va, &rc, values);

        free(pvalues);
        free(values);
        // cleanup only variadic. non-variadic were static
        for (metac_num_t ic = param_count; ic < param_count + variadic_param_count; ++ic ) {_cleanup_ffi_type(args[ic]);}
        free(args);

        return (int)rc;
    } else {
        return _call(p_param_storage_val, fn, p_res_value,
            p_val_list_entries, va_list_number);
    }
    return -(EFAULT);
}

// idea: check all params
// if we see 0 va_list - call directly.
// if we see 1 or more va_list - call special ffi wrapper and 
//    put va_list-internals as ... arg. Wrapper will convert ... to va_list and we can feed it to call
int metac_value_call(metac_value_t * p_param_storage_val, void (*fn)(void), metac_value_t * p_res_value) {
    _check_(
        p_param_storage_val == NULL ||
        metac_value_has_parameter_load(p_param_storage_val) == 0 ||
        fn == NULL, -(EINVAL));

    metac_entry_t *p_param_storage_entry = metac_value_entry(p_param_storage_val);
    _check_(p_param_storage_entry == NULL, -(EFAULT));
    _check_(metac_entry_has_parameters(p_param_storage_entry) == 0, -(EINVAL));

    metac_num_t param_count  = metac_value_parameter_count(p_param_storage_val);
    metac_num_t variadic_param_count = 0;

    if (metac_entry_parameters_count(p_param_storage_entry) < param_count - 1) {
        // something wrong with storage (maybe it wasn't filled)
        return -(EFAULT);
    }

    // calculate how many
    metac_num_t va_list_number = 0;
    metac_num_t va_list_number_cur = 0;
    for (metac_num_t i = 0; i < param_count; ++i) {
        metac_value_t * p_param_val = metac_value_parameter_new_item(p_param_storage_val, i);
        if (p_param_val == NULL) { /* something went wrong */
            return -(EFAULT);
        }
        if (metac_entry_is_va_list_parameter(metac_value_entry(p_param_val)) != 0) {
            ++va_list_number;

        }
        metac_value_delete(p_param_val);
    }

    struct _va_list_entry * p_val_list_entries = NULL;
    if (va_list_number > 0) {
        p_val_list_entries = calloc(va_list_number, sizeof(struct _va_list_entry));
        if (p_val_list_entries == NULL) {
            return -(ENOMEM);
        }

        // put data in:
        for (metac_num_t i = 0; i < param_count; ++i) {
            metac_value_t * p_param_val = metac_value_parameter_new_item(p_param_storage_val, i);
            if (p_param_val == NULL) { /* something went wrong */
                return -(EFAULT);
            }
            if (metac_entry_is_va_list_parameter(metac_value_entry(p_param_val)) != 0) {
                assert(va_list_number_cur < va_list_number);
                p_val_list_entries[va_list_number_cur].id = i;
                p_val_list_entries[va_list_number_cur].p_val = p_param_val;
                ++va_list_number_cur;
            } else {
                metac_value_delete(p_param_val);
            }
        }
    }

    int res = _call_wrapper(
        p_param_storage_val, fn, p_res_value,
        0, p_val_list_entries, va_list_number);

    if (p_val_list_entries != NULL) {
        for (metac_num_t i = 0; i < va_list_number; ++i) {
            if (p_val_list_entries[i].p_val != NULL) {
                metac_value_delete(p_val_list_entries[i].p_val);
                p_val_list_entries[i].p_val = NULL;
            }
        }
        free(p_val_list_entries);
    }
    return res;
}