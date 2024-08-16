#include "metac/reflect.h"

#include <ctype.h> /* isprint */
#include <inttypes.h> /* PRIi8... */
#include <errno.h> /* EINVAL... */

#include "metac/backend/string.h"
#ifndef dsprintf
dsprintf_render_with_buf(64)
#define dsprintf(_x_...) dsprintf64( _x_)
#endif

metac_flag_t metac_entry_is_char(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "char", METAC_ENC_signed_char, sizeof(char)) == 0;
}
metac_flag_t metac_entry_is_uchar(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "unsigned char", METAC_ENC_unsigned_char, sizeof(unsigned char)) == 0;
}
metac_flag_t metac_entry_is_short(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "short int", METAC_ENC_signed, sizeof(short int)) == 0;
}
metac_flag_t metac_entry_is_ushort(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "unsigned short int", METAC_ENC_unsigned, sizeof(unsigned short int)) == 0;
}
metac_flag_t metac_entry_is_int(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "int", METAC_ENC_signed, sizeof(int)) == 0;
}
metac_flag_t metac_entry_is_uint(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "unsigned int", METAC_ENC_unsigned, sizeof(unsigned int)) == 0;
}
metac_flag_t metac_entry_is_long(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "long int", METAC_ENC_signed, sizeof(long int)) == 0;
}
metac_flag_t metac_entry_is_ulong(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "unsigned long int", METAC_ENC_unsigned, sizeof(unsigned long int)) == 0;
}
metac_flag_t metac_entry_is_llong(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "long long int", METAC_ENC_signed, sizeof(long long int)) == 0;
}
metac_flag_t metac_entry_is_ullong(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "unsigned long long int", METAC_ENC_unsigned, sizeof(unsigned long long int)) == 0;
}
metac_flag_t metac_entry_is_bool(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "_Bool", METAC_ENC_boolean, sizeof(bool)) == 0;
}
metac_flag_t metac_entry_is_float(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "float", METAC_ENC_float, sizeof(float)) == 0;
}
metac_flag_t metac_entry_is_double(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "double", METAC_ENC_float, sizeof(double)) == 0;
}
metac_flag_t metac_entry_is_ldouble(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "long double", METAC_ENC_float, sizeof(long double)) == 0;
}
metac_flag_t metac_entry_is_float_complex(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "complex float", METAC_ENC_complex_float, sizeof(float complex)) == 0;
}
metac_flag_t metac_entry_is_double_complex(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "complex double", METAC_ENC_complex_float, sizeof(double complex)) == 0;
}
metac_flag_t metac_entry_is_ldouble_complex(metac_entry_t * p_entry) {
    return metac_entry_check_base_type(p_entry, "long complex double", METAC_ENC_complex_float, sizeof(long double complex)) == 0;
}
/**/
metac_flag_t metac_value_is_char(metac_value_t * p_val) {
    return metac_entry_is_char(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_uchar(metac_value_t * p_val) {
    return metac_entry_is_uchar(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_short(metac_value_t * p_val) {
    return metac_entry_is_short(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_ushort(metac_value_t * p_val) {
    return metac_entry_is_ushort(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_int(metac_value_t * p_val) {
    return metac_entry_is_int(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_uint(metac_value_t * p_val) {
    return metac_entry_is_uint(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_long(metac_value_t * p_val) {
    return metac_entry_is_long(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_ulong(metac_value_t * p_val) {
    return metac_entry_is_ulong(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_llong(metac_value_t * p_val) {
    return metac_entry_is_llong(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_ullong(metac_value_t * p_val) {
    return metac_entry_is_ullong(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_bool(metac_value_t * p_val) {
    return metac_entry_is_bool(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_float(metac_value_t * p_val) {
    return metac_entry_is_float(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_double(metac_value_t * p_val) {
    return metac_entry_is_double(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_ldouble(metac_value_t * p_val) {
    return metac_entry_is_ldouble(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_float_complex(metac_value_t * p_val) {
    return metac_entry_is_float_complex(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_double_complex(metac_value_t * p_val) {
    return metac_entry_is_double_complex(metac_value_entry(p_val));
}
metac_flag_t metac_value_is_ldouble_complex(metac_value_t * p_val) {
    return metac_entry_is_ldouble_complex(metac_value_entry(p_val));
}
/**/
int metac_value_char(metac_value_t * p_val, char *p_var) {
    return metac_value_base_type(p_val, "char", METAC_ENC_signed_char, (void*)p_var, sizeof(*p_var));
}
int metac_value_uchar(metac_value_t * p_val, unsigned char *p_var) {
    return metac_value_base_type(p_val, "unsigned char", METAC_ENC_unsigned_char, (void*)p_var, sizeof(*p_var));
}
int metac_value_short(metac_value_t * p_val, short *p_var) {
    return metac_value_base_type(p_val, "short int", METAC_ENC_signed, (void*)p_var, sizeof(*p_var));
}
int metac_value_ushort(metac_value_t * p_val, unsigned short *p_var) {
    return metac_value_base_type(p_val, "unsigned short int", METAC_ENC_unsigned, (void*)p_var, sizeof(*p_var));
}
int metac_value_int(metac_value_t * p_val, int *p_var) {
    return metac_value_base_type(p_val, "int", METAC_ENC_signed, (void*)p_var, sizeof(*p_var));
}
int metac_value_uint(metac_value_t * p_val, unsigned int *p_var) {
    return metac_value_base_type(p_val, "unsigned int", METAC_ENC_unsigned, (void*)p_var, sizeof(*p_var));
}
int metac_value_long(metac_value_t * p_val, long *p_var) {
    return metac_value_base_type(p_val, "long int", METAC_ENC_signed, (void*)p_var, sizeof(*p_var));
}
int metac_value_ulong(metac_value_t * p_val, unsigned long *p_var) {
    return metac_value_base_type(p_val, "unsigned long int", METAC_ENC_unsigned, (void*)p_var, sizeof(*p_var));
}
int metac_value_llong(metac_value_t * p_val, long long *p_var) {
    return metac_value_base_type(p_val, "long long int", METAC_ENC_signed, (void*)p_var, sizeof(*p_var));
}
int metac_value_ullong(metac_value_t * p_val, unsigned long long *p_var) {
    return metac_value_base_type(p_val, "unsigned long long int", METAC_ENC_unsigned, (void*)p_var, sizeof(*p_var));
}
int metac_value_bool(metac_value_t * p_val, bool *p_var) {
    return metac_value_base_type(p_val, "_Bool", METAC_ENC_boolean, (void*)p_var, sizeof(*p_var));
}
int metac_value_float(metac_value_t * p_val, float *p_var) {
    return metac_value_base_type(p_val, "float", METAC_ENC_float, (void*)p_var, sizeof(float));
}
int metac_value_double(metac_value_t * p_val, double *p_var) {
    return metac_value_base_type(p_val, "double", METAC_ENC_float, (void*)p_var, sizeof(double));
}
int metac_value_ldouble(metac_value_t * p_val, long double *p_var) {
    return metac_value_base_type(p_val, "long double", METAC_ENC_float, (void*)p_var, sizeof(long double));
}
int metac_value_float_complex(metac_value_t * p_val, float complex *p_var) {
    return metac_value_base_type(p_val, "complex float", METAC_ENC_complex_float, (void*)p_var, sizeof(float complex));
}
int metac_value_double_complex(metac_value_t * p_val, double complex *p_var) {
    return metac_value_base_type(p_val, "complex double", METAC_ENC_complex_float, (void*)p_var, sizeof(double complex));
}
int metac_value_ldouble_complex(metac_value_t * p_val, long double complex *p_var) {
    return metac_value_base_type(p_val, "long complex double", METAC_ENC_complex_float, (void*)p_var, sizeof(long double complex));
}
/* */
int metac_value_set_char(metac_value_t * p_val, char var) {
    return metac_value_set_base_type(p_val, "char", METAC_ENC_signed_char, &var, sizeof(var));
}
int metac_value_set_uchar(metac_value_t * p_val, unsigned char var) {
    return metac_value_set_base_type(p_val, "unsigned char", METAC_ENC_unsigned_char, &var, sizeof(var));
}
int metac_value_set_short(metac_value_t * p_val, short var) {
    return metac_value_set_base_type(p_val, "short int", METAC_ENC_signed, &var, sizeof(var));
}
int metac_value_set_ushort(metac_value_t * p_val, unsigned short var) {
    return metac_value_set_base_type(p_val, "unsigned short int", METAC_ENC_unsigned, &var, sizeof(var));
}
int metac_value_set_int(metac_value_t * p_val, int var) {
    return metac_value_set_base_type(p_val, "int", METAC_ENC_signed, &var, sizeof(var));
}
int metac_value_set_uint(metac_value_t * p_val, unsigned int var) {
    return metac_value_set_base_type(p_val, "unsigned int", METAC_ENC_unsigned, &var, sizeof(var));
}
int metac_value_set_long(metac_value_t * p_val, long var) {
    return metac_value_set_base_type(p_val, "long int", METAC_ENC_signed, &var, sizeof(var));
}
int metac_value_set_ulong(metac_value_t * p_val, unsigned long var) {
    return metac_value_set_base_type(p_val, "unsigned long int", METAC_ENC_unsigned, &var, sizeof(var));
}
int metac_value_set_llong(metac_value_t * p_val, long long var) {
    return metac_value_set_base_type(p_val, "long long int", METAC_ENC_signed, &var, sizeof(var));
}
int metac_value_set_ullong(metac_value_t * p_val, unsigned long long var) {
    return metac_value_set_base_type(p_val, "unsigned long long int", METAC_ENC_unsigned, &var, sizeof(var));
}
int metac_value_set_bool(metac_value_t * p_val, bool var) {
    return metac_value_set_base_type(p_val, "_Bool", METAC_ENC_boolean, &var, sizeof(var));
}
int metac_value_set_float(metac_value_t * p_val, float var) {
    return metac_value_set_base_type(p_val, "float", METAC_ENC_float, &var, sizeof(float));
}
int metac_value_set_double(metac_value_t * p_val, double var) {
    return metac_value_set_base_type(p_val, "double", METAC_ENC_float, &var, sizeof(double));
}
int metac_value_set_ldouble(metac_value_t * p_val, long double var) {
    return metac_value_set_base_type(p_val, "long double", METAC_ENC_float, &var, sizeof(long double));
}
int metac_value_set_float_complex(metac_value_t * p_val, float complex var) {
    return metac_value_set_base_type(p_val, "complex float", METAC_ENC_complex_float, &var, sizeof(float complex));
}
int metac_value_set_double_complex(metac_value_t * p_val, double complex var) {
    return metac_value_set_base_type(p_val, "complex double", METAC_ENC_complex_float, &var, sizeof(double complex));
}
int metac_value_set_ldouble_complex(metac_value_t * p_val, long double complex var) {
    return metac_value_set_base_type(p_val, "long complex double", METAC_ENC_complex_float, &var, sizeof(long double complex));
}

int metac_value_num(metac_value_t * p_val, metac_num_t * p_var) {
    if (p_var == NULL) {
        return -(EINVAL);
    }
#define _read_(_type_, _pseudoname_) \
    do { \
        if ( metac_value_is_##_pseudoname_(p_val) != 0) { \
            _type_ v; \
            if (metac_value_##_pseudoname_(p_val, &v) == 0) { \
                *p_var = (metac_num_t)v; \
                return 0;\
            } \
        } \
    } while(0)
    _read_(bool, bool);
    _read_(char, char);
    _read_(unsigned char, uchar);
    _read_(short, short);
    _read_(unsigned short, ushort);
    _read_(int, int);
    _read_(unsigned int, uint);
    _read_(long, long);
    _read_(unsigned long, ulong);
    _read_(long long, llong);
    _read_(unsigned long long, ullong);

#undef _read_
    if (metac_value_is_enumeration(p_val) != 0) {
        metac_const_value_t v;
        if (metac_value_enumeration(p_val, &v) == 0) {
            *p_var = (metac_num_t)v;
            return 0;
        }
    }
    return -(ENOTSUP);
}

int metac_value_equal_base_type(metac_value_t *p_val1, metac_value_t *p_val2) {
    if (p_val1 == NULL || p_val2 == NULL) {
        return -(EINVAL);
    }
#define _cmp_(_type_, _pseudoname_) \
    do { \
        if ( metac_value_is_##_pseudoname_(p_val1) != 0) { \
            if (metac_value_is_##_pseudoname_(p_val2) == 0) { \
                return -(EFAULT); \
            } \
            _type_ v1, v2; \
            if (metac_value_##_pseudoname_(p_val1, &v1) != 0) { \
                return -(EFAULT); \
            } \
            if (metac_value_##_pseudoname_(p_val2, &v2) != 0) { \
                return -(EFAULT); \
            } \
            return (v1 == v2)?1:0; \
        } \
    } while(0)
    _cmp_(char, char);
    _cmp_(unsigned char, uchar);
    _cmp_(short, short);
    _cmp_(unsigned short, ushort);
    _cmp_(int, int);
    _cmp_(unsigned int, uint);
    _cmp_(long, long);
    _cmp_(unsigned long, ulong);
    _cmp_(long long, llong);
    _cmp_(unsigned long long, ullong);
    _cmp_(bool, bool);
    _cmp_(float, float);
    _cmp_(double, double);
    _cmp_(long double, ldouble);
    _cmp_(float complex, float_complex);
    _cmp_(double complex, double_complex);
    _cmp_(long double complex, ldouble_complex);

#undef _cmp_
    return -(ENOTSUP);

}

metac_value_t * metac_value_copy_base_type(metac_value_t *p_src_val, metac_value_t *p_dst_val) {
    if (p_src_val == NULL || p_dst_val == NULL) {
        return NULL;
    }

#define _copy_(_type_, _pseudoname_) \
    do { \
        if ( metac_value_is_##_pseudoname_(p_src_val) != 0) { \
            if (metac_value_is_##_pseudoname_(p_dst_val) == 0) { \
                return NULL; \
            } \
            _type_ v; \
            if (metac_value_##_pseudoname_(p_src_val, &v) != 0) { \
                return NULL; \
            } \
            if (metac_value_set_##_pseudoname_(p_dst_val, v) != 0) { \
                return NULL; \
            } \
            return p_dst_val; \
        } \
    } while(0)

    _copy_(char, char);
    _copy_(unsigned char, uchar);
    _copy_(short, short);
    _copy_(unsigned short, ushort);
    _copy_(int, int);
    _copy_(unsigned int, uint);
    _copy_(long, long);
    _copy_(unsigned long, ulong);
    _copy_(long long, llong);
    _copy_(unsigned long long, ullong);
    _copy_(bool, bool);
    _copy_(float, float);
    _copy_(double, double);
    _copy_(long double, ldouble);
    _copy_(float complex, float_complex);
    _copy_(double complex, double_complex);
    _copy_(long double complex, ldouble_complex);

#undef _copy_
    return NULL;
}

char *metac_value_base_type_string(metac_value_t * p_val) {
    if (metac_value_is_char(p_val) != 0) {
        char v;
        if (metac_value_char(p_val, &v) != 0) {
            return NULL;
        }
        if (isprint(v)) {
            return dsprintf("'%c'", v);
        }
        return dsprintf("%"PRIi8, v);
    }
#define _string_(_type_, _pseudoname_, _dprint_expr_...) \
    do { \
        if ( metac_value_is_##_pseudoname_(p_val) != 0) { \
            _type_ v; \
            if (metac_value_##_pseudoname_(p_val, &v) != 0) { \
                return NULL; \
            } \
            return dsprintf(_dprint_expr_); \
        } \
    } while(0)
    _string_(unsigned char, uchar, "%"PRIu8, v);
    _string_(short, short, "%hi", v);
    _string_(unsigned short, ushort, "%hu", v);
    _string_(int, int, "%i", v);
    _string_(unsigned int, uint, "%u", v);
    _string_(long, long, "%li", v);
    _string_(unsigned long, ulong, "%lu", v);
    _string_(long long, llong, "%Li", v);
    _string_(unsigned long long, ullong, "%Lu", v);
    _string_(bool, bool, "%s", (v==true)?"true":"false");
    _string_(float, float, "%f", v);
    _string_(double, double, "%lf", v);
    _string_(long double, ldouble, "%Lf", v);
#undef _string_
#define _string_(_type_, _pseudoname_) \
    do { \
        if ( metac_value_is_##_pseudoname_(p_val) != 0) { \
            _type_ v; \
            if (metac_value_##_pseudoname_(p_val, &v) != 0) { \
                return NULL; \
            } \
            if (cimag(v) < 0.0) { \
                return dsprintf( "%lf - I * %lf", creal(v), -1.0 * cimag(v)); \
            } \
            return dsprintf( "%lf + I * %lf", creal(v), cimag(v)); \
        } \
    } while(0)
    _string_(float complex, float_complex);
    _string_(double complex, double_complex);
    _string_(long double complex, ldouble_complex);
#undef _string_


    return NULL;
}
