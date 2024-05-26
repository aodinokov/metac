#ifndef INCLUDE_METAC_REFLECT_DB_H_
#define INCLUDE_METAC_REFLECT_DB_H_

#include "metac/reqresp.h"
#include "metac/reflect/entry.h"
#include "metac/reflect/module.h"

/* 
Macroses to export C type definitions and their params in code.
*/
struct name_to_entries_info {
        metac_name_t name;
        metac_num_t entries_count;
        metac_entry_t **entries;
};
struct name_to_entries {
    metac_num_t items_count;
    struct name_to_entries_info *items;
};
struct db_v0 {
    // all possible entries kinds with names
    struct name_to_entries base_types;
    struct name_to_entries enums;
    struct name_to_entries typedefs;
    struct name_to_entries structs;
    struct name_to_entries unions;
    struct name_to_entries classes;
    struct name_to_entries namespaces;
    struct name_to_entries variables;
    struct name_to_entries subprograms;
    struct name_to_entries compile_units;
};
struct metac_db {
    metac_num_t version;
    union {
        struct db_v0 db_v0;
    };
};
struct metac_db_link {
    struct metac_db ** pp_db;
};

#define METAC_DB_NAME() METAC_MODULE_RESPONSE(METAC_MODULE_NAME, db, dflt)
#define METAC_DB_LINK_NAME() METAC_MODULE_REQUEST(METAC_MODULE_NAME, db, dflt)


#ifndef _METAC_OFF_
#define _METAC_DB_LINK() \
    extern struct metac_db *METAC_DB_NAME(); \
    struct metac_db_link METAC_DB_LINK_NAME()  = {.pp_db = &METAC_DB_NAME(),}
#else
#define _METAC_DB_LINK() \
    struct metac_db_link METAC_DB_LINK_NAME()  = {.pp_db = NULL}
#endif /* _METAC_OFF_ */
#define METAC_DB_LINK() _METAC_DB_LINK()

#define METAC_DB_LINK_PTR() ({ \
        struct metac_db * p_db = NULL; \
        if (METAC_DB_LINK_NAME().pp_db != NULL) { \
            p_db = *(METAC_DB_LINK_NAME().pp_db); \
        } \
        p_db; \
    })

#endif