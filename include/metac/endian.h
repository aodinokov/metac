#ifndef INCLUDE_METAC_ENDIAN_H_
#define INCLUDE_METAC_ENDIAN_H_

/** anon union to identify if the order is big or little endian */
static union {
    int value; /**< set int value, */
    char bytes[sizeof(int)]; /**< but read as bytes */
} const _endian_order_test = { .value = 0x7f };
/** 
 * @brief true if we run on little endian architecture
 * e.g. x86 is little endian 
 */
#define IS_LITTLE_ENDIAN (_endian_order_test.bytes[0] != 0)

#endif