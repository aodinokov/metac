#ifndef INCLUDE_METAC_REFLECT_LOC_H_
#define INCLUDE_METAC_REFLECT_LOC_H_

#include "metac/reqresp.h"
#include "metac/reflect/entry.h"
#include "metac/reflect/module.h"

/* deliver via function that accepts db and location */

/** @brief same-name entries stored in db with the same name, number and pointer to array of pointer*/
struct metac_per_name_item {
    metac_name_t name;  /**< name of entry */
    metac_num_t entry_count; /**< len of pp_entries (typically must be 1 )*/
    metac_entry_t **pp_entries; /**< all pointers */
};

/** @brief all entries sorted by name which are located with declloc on the same line */
struct metac_per_loc_item {
    const metac_name_t loc_name; /**< name of declloc */
    metac_num_t per_name_item_count; /**< len of per_name_items */
    struct metac_per_name_item * per_name_items; /** */
};

/** TODO: change sort order -> by reverse string. This will allow to compare by the local path 
 * just compare from tail.
 * part of the path must be precise. if path is local -the next in the bigger string must be / (or \)
*/

/** @brief all entries sorted by declloc name which are located in the same file */
struct metac_per_file_item {
    metac_name_t file; /**< filename where item is located*/
    metac_num_t per_loc_item_count; /**< len of per_loc_items */
    struct metac_per_loc_item * per_loc_items; /**< declloc items */
};

/** @brief top level of declloc entry db */
struct metac_entry_db {
    metac_num_t per_file_item_count; /**< len of per_file_items */
    struct metac_per_file_item *per_file_items; /**< all files */
};

/** @brief struct to keep decl location */
struct metac_decl_loc {
        const metac_name_t file;    /**< filename where this item is declared */
        const metac_name_t name;    /**< name of declloc (without additional postfix) */
};

/** @brief find entry in the given declloc entry db
 *  @param p_entry_db db where we look
 *  @param p_decl_loc declloc pointer
 *  @param varname name of the variable
 *  @return entry of the variable. NULL if wasn't able to find.
*/
extern metac_entry_t * metac_entry_from_entry_db(
    struct metac_entry_db * p_entry_db,
    struct metac_decl_loc * p_decl_loc,
    const metac_name_t varname);

#define METAC_ENTRY_DB_NAME(name) METAC_RESPONSE(entry, name) /*it's ok ot have response not module type. reponse is generated per many requests */
#define METAC_DECLLOC(name) METAC_MODULE_REQUEST(METAC_MODULE_NAME, entry, name)
#define METAC_DECLLOC_PTR(name) METAC_MODULE_RESPONSE(METAC_MODULE_NAME, entry, name)

/* _name_ must be unique within the same line in the file.. better if within function for local vars */
#define ___WITH_METAC_DECLLOC(_mod_name_, _local_name_, _postfix_, _declaration_...) \
        struct metac_decl_loc METAC_DECLLOC(_local_name_ ## _ ## _postfix_) = { \
            .file = __FILE__, \
            .name = "metac__" \
                    # _mod_name_ \
                    "_entry_" \
                    # _local_name_ "_" \
                    # _postfix_, \
        }; \
        struct metac_decl_loc * METAC_DECLLOC_PTR(_local_name_) = &METAC_DECLLOC(_local_name_ ## _ ## _postfix_); \
        _declaration_
#define __WITH_METAC_DECLLOC(_mod_name_, _local_name_, _postfix_, _declaration_...) ___WITH_METAC_DECLLOC(_mod_name_, _local_name_, _postfix_, _declaration_)
#define _WITH_METAC_DECLLOC(_name_, _declaration_...) __WITH_METAC_DECLLOC(METAC_MODULE_NAME , _name_, __COUNTER__, _declaration_)
/** @brief make _declaration_ on the same line as declloc with _name_. all variables located on the line will be in db */
#define WITH_METAC_DECLLOC(_name_, _declaration_...) _WITH_METAC_DECLLOC(_name_, _declaration_)

#ifndef _METAC_OFF_
#define METAC_ENTRY_DB METAC_ENTRY_DB_NAME(METAC_MODULE_NAME)
extern struct metac_entry_db * METAC_ENTRY_DB;
#else
#define METAC_ENTRY_DB NULL
#endif

/** @brief request entry information from declloc/variable name pair */
#define METAC_ENTRY_FROM_DECLLOC(_name_, _var_) \
        metac_entry_from_entry_db(METAC_ENTRY_DB, \
        METAC_DECLLOC_PTR(_name_), \
        #_var_)

#endif