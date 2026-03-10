// include/metac/backend/cjson.h
#ifndef METAC_BACKEND_CJSON_H
#define METAC_BACKEND_CJSON_H

#include "metac/reflect.h"
#include "metac/serialization/common.h"

// Forward declare cJSON type
struct cJSON;

/** @brief Convert a metac_value_t to a cJSON object.
 *  @param p_val The value to convert.
 *  @param wmode The walk mode (shallow or deep).
 *  @param p_tag_map Tagmap to resolve ambiguities.
 *  @return A cJSON object, or NULL on failure.
 */
struct cJSON* metac_value_to_cjson(metac_value_t* p_val, metac_value_walk_mode_t wmode, metac_tag_map_t* p_tag_map);

/** @brief Populate a metac_value_t from a cJSON object.
 *  @param p_val The destination value to populate.
 *  @param json The source cJSON object.
 *  @param p_mode mode in which this function will work. if NULL - uses default with fail on all unknown situations
 *  @param calloc_fn address of function which can be used for allocating. if NULL - use calloc
 *  @param free_fn address of function which can be used for freeing memory (in case of failure during execution). if NULL - uses free
 *  @param p_tag_map Tagmap to resolve ambiguities.
 *  @return 0 on success, negative on failure.
 */
int metac_value_from_cjson(metac_value_t* p_val, struct cJSON* json, 
    metac_value_deserialization_mode_t * p_mode,
    void *(*calloc_fn)(size_t nmemb, size_t size),
    void (*free_fn)(void *ptr), /* free used in case of failure */
    metac_tag_map_t* p_tag_map);

#endif // METAC_BACKEND_CJSON_H
