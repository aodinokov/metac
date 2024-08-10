#include "value_ffi.h"

#include "metac/backend/helpers.h"

#include <assert.h>
#include <ffi.h>
#include <stdlib.h> /*calloc, free*/


// int _call_val(void (*fn)(void),/* metac_value_t * p_val*/ int a, short b) {
//     ffi_cif cif;
//     ffi_type *args[] = {
//         &ffi_type_sint,
//         &ffi_type_sshort
//     };
    
//     /* Initialize the cif */
//     if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, 
// 		       &ffi_type_sint, args) == FFI_OK) {

//         ffi_arg rc;

//         void *values[] = {
//             &a, &b,
//         };

//         ffi_call(&cif, fn, &rc, values);
//         return (int)rc;
//     }
//     return -1;
// }

metac_value_t * metac_value_call(metac_value_t * p_param_storage_val, void (*fn)(void)) {
    _check_(
        p_param_storage_val == NULL ||
        metac_value_has_parameter_load(p_param_storage_val) == 0 ||
        fn == NULL, NULL);    

    metac_num_t param_count  = metac_value_parameter_count(p_param_storage_val);
    metac_num_t variadic_param_count = -1; // fn is variadic if >=0 (0 is extreme case e.g. printf("string"))

    ffi_type **args = NULL;
    if (param_count > 0) {
        // check the last param if it's variadic
        metac_value_t * p_last_param_val = metac_value_parameter_new_item(p_param_storage_val, param_count - 1);
        if (p_last_param_val == NULL) { /* something went wrong */
            return NULL;
        }

        if (metac_entry_is_unspecified_parameter(metac_value_entry(p_last_param_val)) != 0) {
            assert(metac_value_has_parameter_load(p_last_param_val) != 0);
            variadic_param_count = metac_value_parameter_count(p_last_param_val);
        }
        // we don't need it for now
        metac_value_delete(p_last_param_val);
    }   

    ffi_cif cif;

    // ffi handles variadic and non-variadic in different way
    if (variadic_param_count < 0) { // non variadic fn
        // types
        ffi_type **args = NULL;
        // vals
        void **values = NULL;

        if (param_count > 0) {
            args = calloc(param_count, sizeof(ffi_type *));
            if (args == NULL) {
                return NULL;
            }
            values = calloc(param_count, sizeof(void *));
            if (values == NULL) {
                free(args);
                return NULL;
            }
            for (metac_num_t i = 0 ; i < param_count; ++i) {
                metac_value_t * p_param_val = metac_value_parameter_new_item(p_param_storage_val, i);
                if (p_param_val == NULL) {
                    free(values);
                    free(args);
                    return NULL;
                }
                // fill in type
                if (metac_value_is_base_type(p_param_val) != 0) {
#define _process_(_metac_type_, _ffi_type_) \
                    if (metac_value_is_##_metac_type_(p_param_val)) { \
                        args[i] = &_ffi_type_; \
                    } 
                    _process_(char, ffi_type_schar);
                    _process_(uchar, ffi_type_uchar);
                    _process_(short, ffi_type_sshort);
                    _process_(ushort, ffi_type_ushort);
                    _process_(int, ffi_type_sint);
                    _process_(uint, ffi_type_uint);
                    _process_(long, ffi_type_slong);
                    _process_(ulong, ffi_type_ulong);
                    _process_(llong, ffi_type_slong); // ??? not sure
                    _process_(ullong, ffi_type_ulong); // ??? not sure
                    _process_(bool, ffi_type_uchar);
                    _process_(float, ffi_type_float);
                    _process_(double, ffi_type_double);
                    _process_(ldouble, ffi_type_longdouble);
                    _process_(float_complex, ffi_type_complex_float);
                    _process_(double_complex, ffi_type_complex_double);
                    _process_(ldouble_complex, ffi_type_complex_longdouble);
#undef _process_
                } else if (metac_value_is_enumeration(p_param_val) != 0) {
                    metac_size_t param_byte_sz = 0;
                    if (metac_entry_byte_size(metac_value_entry(p_param_val), &param_byte_sz) != 0) {
                        metac_value_delete(p_param_val);
                        free(values);
                        free(args);
                        return NULL;
                    }
#define _process_(_type_size_, _ffi_type_) \
                    if (param_byte_sz == sizeof(_type_size_)) { \
                        args[i] = &_ffi_type_; \
                    } 
                    _process_(char, ffi_type_schar);
                    _process_(short, ffi_type_sshort);
                    _process_(int, ffi_type_sint);
                    _process_(long, ffi_type_slong);
                    _process_(long long, ffi_type_slong);
#undef _process_
                } else if (metac_value_is_pointer(p_param_val) != 0) {
                    args[i] = &ffi_type_pointer;
                }

                // fill in data
                values[i] = metac_value_addr(p_param_val);

                metac_value_delete(p_param_val);
            }
        }

        if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, param_count, 
		       &ffi_type_sint, // TODO: this is to be defined
                args) == FFI_OK) {
            ffi_arg rc;
            ffi_call(&cif, fn, &rc, values);
            return NULL;
        }
    } else { // TBD

    }

    return NULL;
}