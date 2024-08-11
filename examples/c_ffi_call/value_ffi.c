#include "value_ffi.h"

#include "metac/backend/helpers.h"

#include <assert.h>
#include <errno.h>
#include <ffi.h>
#include <stdlib.h> /*calloc, free*/

static int _val_to_ffi_type(metac_value_t * p_val, ffi_type ** pp_rtype) {

    if (metac_value_is_base_type(p_val) != 0) {

#define _process_(_metac_type_, _ffi_type_) \
        if (metac_value_is_##_metac_type_(p_val)) { \
            *pp_rtype = &_ffi_type_; \
        } 

        _process_(char, ffi_type_schar);
        _process_(uchar, ffi_type_uchar);
        _process_(short, ffi_type_sshort);
        _process_(ushort, ffi_type_ushort);
        _process_(int, ffi_type_sint);
        _process_(uint, ffi_type_uint);
        _process_(long, ffi_type_slong);
        _process_(ulong, ffi_type_ulong);
        _process_(llong, ffi_type_slong); // TODO: check
        _process_(ullong, ffi_type_ulong); // TODO: check
        _process_(bool, ffi_type_uchar);
        _process_(float, ffi_type_float);
        _process_(double, ffi_type_double);
        _process_(ldouble, ffi_type_longdouble);
        _process_(float_complex, ffi_type_complex_float);
        _process_(double_complex, ffi_type_complex_double);
        _process_(ldouble_complex, ffi_type_complex_longdouble);
#undef _process_
    } else if (metac_value_is_enumeration(p_val) != 0) {
        metac_size_t param_byte_sz = 0;
        if (metac_entry_byte_size(metac_value_entry(p_val), &param_byte_sz) != 0) {
            return -(EFAULT);
        }

#define _process_(_type_size_, _ffi_type_) \
        if (param_byte_sz == sizeof(_type_size_)) { \
            *pp_rtype = &_ffi_type_; \
        } 

        _process_(char, ffi_type_schar);
        _process_(short, ffi_type_sshort);
        _process_(int, ffi_type_sint);
        _process_(long, ffi_type_slong);
        _process_(long long, ffi_type_slong);
#undef _process_
    } else if (
        metac_value_is_pointer(p_val) != 0 ||
        metac_value_has_elements(p_val) != 0 /*array*/) {
        *pp_rtype = &ffi_type_pointer;
    } else { //TODO: struct

            return -(ENOTSUP);
    }
    return 0;
}

static int _ffi_arg_to_value(ffi_arg arg, metac_value_t * p_val) {
    if (p_val == NULL) {
        return -(EINVAL);
    }
    if (metac_value_is_base_type(p_val) != 0) {
#define _process_(_type_, _pseudoname_) \
        do { \
            if ( metac_value_is_##_pseudoname_(p_val) != 0) { \
                _type_ v = (_type_)arg; \
                if (metac_value_set_##_pseudoname_(p_val, v) == 0) { \
                    return 0;\
                } \
            } \
        } while(0)

        _process_(bool, bool);
        _process_(char, char);
        _process_(unsigned char, uchar);
        _process_(short, short);
        _process_(unsigned short, ushort);
        _process_(int, int);
        _process_(unsigned int, uint);
        _process_(long, long);
        _process_(unsigned long, ulong);
        _process_(long long, llong);
        _process_(unsigned long long, ullong);

#undef _process_
        return -(ENOTSUP);
    }
    if (metac_value_is_enumeration(p_val) != 0) {
        metac_const_value_t v = (metac_const_value_t)arg;
        if (metac_value_set_enumeration(p_val, v) == 0) {
            return 0;
        }
    }
    // TODO: set ptr, array, struct?
    return -(ENOTSUP);
}

void metac_value_with_call_parameters_delete(metac_value_t * p_param_value) {
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

        int fill_in_res = _val_to_ffi_type(p_res_value, &rtype);
        if (fill_in_res != 0) {
            return -(EFAULT);
        }
    }

    metac_num_t param_count  = metac_value_parameter_count(p_param_storage_val);
    metac_num_t variadic_param_count = -1; // fn is variadic if >=0 (0 is extreme case e.g. printf("string"))

    ffi_type **args = NULL;
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
        // types
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
                    free(args);
                    return -(EFAULT);
                }
                // fill in type
                int fill_in_res = _val_to_ffi_type(p_param_val, &args[i]);
                if (fill_in_res != 0) {
                    metac_value_delete(p_param_val);
                    free(values);
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
            free(args);
            return -(EFAULT);
        }

        ffi_call(&cif, fn,
            (metac_entry_has_result(p_param_storage_entry) == 0)?NULL:&rc,
            values);

        free(values);
        free(args);
    } else {
        // TODO: variadic
        return -(ENOTSUP);
    }

    if (metac_entry_has_result(p_param_storage_entry) != 0 && p_res_value != NULL) {
        int conv = _ffi_arg_to_value(rc, p_res_value);
        assert(conv == 0);
    }
    
    return 0;
}