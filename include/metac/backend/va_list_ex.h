#ifndef VA_LIST_EX_H_
#define VA_LIST_EX_H_

#include "metac/base.h"

#include <stdlib.h> /*alloca*/

/** @brief wrapper macro to put some things into va_list
    example how to get va_list using all this combination of macroses
    ```
    WITH_VA_LIST_CONTAINER(c,
        vsnprintf(buf, sizeof(buf), "%d %d %d %d %d %d %d %d %d %d", VA_LIST_FROM_CONTAINER(c, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
    );
    ```
    it outputs 1 2 3 4 5 6 7 8 9 10

    WARNING: even though it works here - there is no guarantee that it will work everywhere and with everything.
    Try to avoid using this beyond some tests. Metac uses this only for writing tests for `va_list`. Reason:
    A `va_list` is intrinsically linked to a specific function call's stack frame.
    Creating a container to hold it doesn't change this fundamental characteristic.
    Accessing it in a different function would involve undefined behavior due to potential stack frame changes.
*/
#define WITH_VA_LIST_CONTAINER(_name_, _in_...) do { \
        struct va_list_container _name_; \
        void * _ptr_ = NULL; \
        _in_ \
        va_end(_name_.parameters); \
    }while(0)

/** @brief this must be big enough to cover stack for alloca */
#define _va_list_padding (int)0x55AAAA55, (int)0x55AAAA55, (int)0x55AAAA55, (int)0x55AAAA55

/** @brief helper that sets va_list start position and stores the pointer with the top address
 * (note that in C parameters are getting added from the last one, that's why pp will be the last
 * but also note that stack grows in backward direction from higher addr to lower)
 */
static inline void va_list_container_start(void** pp, struct va_list_container * p, ...) {
    *pp = (void*)&(pp); // store the top stack param addr
    va_start(p->parameters, p);
}
/** @brief macro to get va_list from container and protect it with alloca 
 * alternativly we could use 2 alloca - first is to get the original stack addr
 * and second is to cover pp value (which is the top of the used by va_list_container_start stack)
 * this will be more precise 
*/
#define VA_LIST_CONTAINER(_name_, _args_...) ({ \
        void* pp = NULL; \
        va_list_container_start(&pp, &_name_, _args_, _va_list_padding); \
        _ptr_ = alloca(((char*)&pp)-((char*)pp)); \
        &_name_; \
    })
/** @brief additional helper macro which returst va_list */
#define VA_LIST_FROM_CONTAINER(_name_, _args_...) VA_LIST_CONTAINER(_name_, _args_)->parameters

#endif