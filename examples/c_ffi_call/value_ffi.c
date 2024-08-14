#include "value_ffi.h"

#include "metac/backend/helpers.h"

#include <assert.h>
#include <errno.h>
#include <ffi.h>
#include <stdlib.h> /*calloc, free*/
#include <string.h>

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


// convert function parameters types to ffi_type (metac->ffi)
static int _val_to_ffi_type(metac_entry_t * p_entry, ffi_type ** pp_rtype) {

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
        metac_entry_is_pointer(p_entry) != 0) {
        ffi_type * p_ffi_type = calloc(1, sizeof(ffi_type));
        if (p_ffi_type == NULL) {
            return -ENOMEM;
        }
        memcpy(p_ffi_type, &ffi_type_pointer, sizeof(ffi_type));
        *pp_rtype = p_ffi_type;
        return 0;
    } else if (metac_entry_has_members(p_entry)) {
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
                }
            }
            assert(p_largest_entry != NULL);
            return _val_to_ffi_type(p_largest_entry, pp_rtype);
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
        metac_offset_t current_offset, last_offset = 0;
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

                current_offset = bitfields_raw_info.byte_offset;
            } else { // bitfield
                metac_offset_t byte_offset = 0, bit_offset = 0, bit_size = 0;
                if (metac_entry_member_bitfield_offsets(p_memb_entry, &byte_offset, &bit_offset, &bit_size) != 0) {
                    free(p_tmp->elements);
                    free(p_tmp);
                    return -(EFAULT);
                }
                current_offset = byte_offset;
            }

            if (i != 0 && last_offset == current_offset) {
                continue; // just skip this element
            }

            if (_val_to_ffi_type(p_memb_entry, &p_tmp->elements[memb_id]) != 0) {
                free(p_tmp->elements);
                free(p_tmp);
                return -(EFAULT);
            }

            last_offset = current_offset;
            ++memb_id;
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
        if (metac_entry_byte_size(p_entry, &e_sz) != 0) {
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
            if (_val_to_ffi_type(p_el_entry, &p_tmp->elements[el_id]) != 0) {
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

metac_value_t * metac_new_value_with_call_params(metac_entry_t *p_entry) {
    metac_value_t * p_val = NULL;
    metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage();
    if (p_param_storage != NULL) {
        p_val = metac_new_value(p_entry, p_param_storage);
    }
    return p_val;
}

void metac_value_with_call_params_delete(metac_value_t * p_param_value) {
    metac_parameter_storage_t * p_param_storage = (metac_parameter_storage_t *)metac_value_addr(p_param_value);
    metac_value_delete(p_param_value);
    metac_parameter_storage_delete(p_param_storage);
}

metac_value_t * metac_new_value_with_call_result(metac_value_t * p_param_storage_val) {
    _check_(
        p_param_storage_val == NULL ||
        metac_value_has_parameter_load(p_param_storage_val) == 0, NULL);

    metac_value_t * p_res_value = NULL;
    metac_size_t res_sz = 0;

    metac_entry_t *p_param_storage_entry = metac_value_entry(p_param_storage_val);
    _check_(p_param_storage_entry == NULL, NULL);
    _check_(metac_entry_has_parameters(p_param_storage_entry) == 0, NULL);

    if (metac_entry_has_result(p_param_storage_entry) != 0) {
        metac_entry_t *p_res_entry = metac_entry_result_type(p_param_storage_entry);
        _check_(p_res_entry == NULL, NULL);

        if (metac_entry_byte_size(p_res_entry, &res_sz) != 0) {
            return NULL;
        }
        _check_(res_sz <= 0, NULL);

        void * p_res_mem = calloc(1, res_sz); // make alloca maybe
        p_res_value = metac_new_value(p_res_entry, p_res_mem);
        if (p_res_value == NULL) {
            free(p_res_mem);
            return NULL;
        }
    }

    return p_res_value;
}

void metac_value_with_call_result_delete(metac_value_t * p_res_value) {
    if (p_res_value == NULL) {
        return;
    }
    free(metac_value_addr(p_res_value));
    metac_value_delete(p_res_value);
}

int metac_value_call(metac_value_t * p_param_storage_val, void (*fn)(void), metac_value_t * p_res_value) {
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

    if (metac_entry_has_result(p_param_storage_entry) != 0) {
        if (p_res_value == NULL) {
            return -(EINVAL);
        }

        int fill_in_res = _val_to_ffi_type(metac_value_entry(p_res_value), &rtype);
        if (fill_in_res != 0) {
            return -(EFAULT);
        }
    }

    metac_num_t param_count  = metac_value_parameter_count(p_param_storage_val);
    metac_num_t variadic_param_count = -1; // fn is variadic if >=0 (0 is extreme case e.g. printf("string"))

    if (param_count > 0) { // check the last param if it's variadic
        metac_value_t * p_last_param_val = metac_value_parameter_new_item(p_param_storage_val, param_count - 1);
        if (p_last_param_val == NULL) { /* something went wrong */
            return -(EFAULT);
        }

        if (metac_entry_is_unspecified_parameter(metac_value_entry(p_last_param_val)) != 0) {
            assert(metac_value_has_parameter_load(p_last_param_val) != 0);
            variadic_param_count = metac_value_parameter_count(p_last_param_val);
        }
        // we don't need it for now
        metac_value_delete(p_last_param_val);
    }

    // ffi handles variadic and non-variadic in different way
    if (variadic_param_count < 0) { // non variadic fn
        // args
        ffi_type **args = NULL;
        // vals
        void **values = NULL;

        if (param_count > 0) {
            args = calloc(param_count, sizeof(ffi_type *));
            if (args == NULL) {
                return -(EFAULT);
            }
            values = calloc(param_count, sizeof(void *));
            if (values == NULL) {
                free(args);
                return -(EFAULT);
            }
            for (metac_num_t i = 0 ; i < param_count; ++i) {
                metac_value_t * p_param_val = metac_value_parameter_new_item(p_param_storage_val, i);
                if (p_param_val == NULL) {
                    free(values);
                    for (metac_num_t ic = 0; ic <= i; ++ic ) {_cleanup_ffi_type(args[ic]);}
                    free(args);
                    return -(EFAULT);
                }
                // fill in type
                int fill_in_res = _val_to_ffi_type(metac_value_entry(p_param_val), &args[i]);
                if (fill_in_res != 0) {
                    metac_value_delete(p_param_val);
                    free(values);
                    for (metac_num_t ic = 0; ic <= i; ++ic ) {_cleanup_ffi_type(args[ic]);}
                    free(args);
                    return fill_in_res;
                }

                // fill in data
                values[i] = metac_value_addr(p_param_val);

                metac_value_delete(p_param_val);
            }
        }

        if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, param_count, rtype, args) != FFI_OK) {

            free(values);
            for (metac_num_t ic = 0; ic < param_count; ++ic ) {_cleanup_ffi_type(args[ic]);}
            free(args);
            return -(EFAULT);
        }

        void * addr = metac_value_addr(p_res_value);
        ffi_call(&cif, fn,
            (metac_entry_has_result(p_param_storage_entry) == 0 || p_res_value == NULL)?NULL:addr,
            values);

        free(values);
        for (metac_num_t ic = 0; ic < param_count; ++ic ) {_cleanup_ffi_type(args[ic]);}
        free(args);
    } else {
        // TODO: variadic
        return -(ENOTSUP);
    }
    
    _cleanup_ffi_type(rtype);
    return 0;
}