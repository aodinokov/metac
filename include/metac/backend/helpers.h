#ifndef INCLUDE_METAC_BACKEND_CHECK_H_
#define INCLUDE_METAC_BACKEND_CHECK_H_

#define _check_(_cond_, _return_code_) \
    do { \
        if (_cond_) { \
            return (_return_code_); \
        } \
    } while(0)

#endif