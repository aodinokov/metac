#ifndef INCLUDE_METAC_REFLECT_LINK_H_
#define INCLUDE_METAC_REFLECT_LINK_H_

#include "metac/reqresp.h"
#include "metac/reflect/entry.h"
#include "metac/reflect/module.h"

/* deliver entries via linker */

#define METAC_GSYM_NAME(name) METAC_MODULE_RESPONSE(METAC_MODULE_NAME, gsym, name)

/** @brief link stucture to request direct pointer to entry for global symbol */
struct metac_entry_link {
    metac_entry_t ** pp_entry;  /**< pointer to the pointer to the entry of global symbol */
    void * p_data;  /** pointer to the global symbol itself (this is to make it work even for static global symbols so they won't ge optimized if not used)*/
};
/*
 * we merged subroutines and variables into gsym because you can't compile
 * int x(){return 0;}
 * int x;
 * In C++ it is possible most likely. at leas there can be 2 fn
 * with the same name and different params and different linkage name.
 * for those cases we may want to introduce links with several results - will see
*/
#define METAC_GSYM_LINK_NAME(name) METAC_MODULE_REQUEST(METAC_MODULE_NAME, gsym, name)

#ifndef _METAC_OFF_
#define _METAC_GSYM_LINK(name) \
    extern metac_entry_t *METAC_GSYM_NAME(name); \
    struct metac_entry_link METAC_GSYM_LINK_NAME(name) = {.pp_entry = &METAC_GSYM_NAME(name), .p_data = (void *)&(name)}
#else
#define _METAC_GSYM_LINK(name) \
    struct metac_entry_link METAC_GSYM_LINK_NAME(name) = {.pp_entry = NULL, .p_data = (void *)&(name)}
#endif /* _METAC_OFF_ */

#define METAC_GSYM_LINK(name) _METAC_GSYM_LINK(name)

/** @brief use this marcos to request entry for the given global symbol (variable or subroutine) */
#define METAC_GSYM_LINK_ENTRY(_name_) ({ \
        metac_entry_t * p_entry = NULL; \
        if (METAC_GSYM_LINK_NAME(_name_).pp_entry != NULL) { \
            p_entry = *(METAC_GSYM_LINK_NAME(_name_).pp_entry); \
        } \
        p_entry; \
    })

#endif