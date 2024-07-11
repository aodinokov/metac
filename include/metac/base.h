#ifndef INCLUDE_METAC_BASE_H_
#define INCLUDE_METAC_BASE_H_

#include <stdio.h>  /* NULL */
#include <stdarg.h> /* va_list */
#include <stdint.h> /* int8_t, uint8_t, uint64_t */
#include <inttypes.h> /* PRId64, ...*/
#include <stdbool.h> /* bool */
#include <complex.h> /* complex */

#include "metac/const.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief the cross-platform way to pass va_list back and forth in functions */
struct va_list_container {
    va_list parameters;
};

/** @brief kind of metac_entry_t */
typedef int8_t metac_kind_t;

/** @brief len of array */
typedef int metac_num_t;

/** @brief definition of types used for attributes */
typedef char *metac_name_t;

#define METAC_FLAG_PRI PRIu8
#define METAC_FLAG_PRIo PRIo8
#define METAC_FLAG_PRIx PRIx8
/** @brief flag (0 - false or non-zero - true) */
typedef uint8_t metac_flag_t;

#define METAC_ENCODING_PRI PRIu8
#define METAC_ENCODING_PRIo PRIo8
#define METAC_ENCODING_PRIx PRIx8
/** @brief encoding - see METAC_ENC_... values */
typedef uint8_t metac_encoding_t;

#define METAC_LANG_PRI PRIu8
#define METAC_LANG_PRIo PRIo8
#define METAC_LANG_PRIx PRIx8
/** @brief language - see METAC_LANG... values */
typedef uint8_t metac_lang_t;

#define METAC_CONST_VALUE_PRI PRId64
#define METAC_CONST_VALUE_PRIo PRIo64
#define METAC_CONST_VALUE_PRIx PRIx64
/** @brief value of enum entry */
typedef int64_t metac_const_value_t;

#define METAC_BOUND_PRI PRId64
#define METAC_BOUND_PRIo PRIo64
#define METAC_BOUND_PRIx PRIx64
/** @brief arrays index */
typedef int64_t metac_bound_t;

#define METAC_COUNT_PRI PRId64
#define METAC_COUNT_PRIo PRIo64
#define METAC_COUNT_PRIx PRIx64
/** @brief just some int to return length */
typedef int64_t metac_count_t;

#define METAC_SIZE_PRI PRIu64
#define METAC_SIZE_PRIo PRIo64
#define METAC_SIZE_PRIx PRIx64
/** @brief similar to size_t */
typedef uint64_t metac_size_t;

#define METAC_OFFSET_PRI PRIu64
#define METAC_OFFSET_PRIo PRIo64
#define METAC_OFFSET_PRIx PRIx64
/** @brief offset of member within struct (this will be used with some pointer math)*/
typedef uint64_t metac_offset_t;


#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_METAC_BASE_H_ */
