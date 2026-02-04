// include/metac/backend/cjson.h
#ifndef METAC_BACKEND_CJSON_H
#define METAC_BACKEND_CJSON_H

#include "metac/reflect.h"

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
 *  @param p_tag_map Tagmap to resolve ambiguities.
 *  @return 0 on success, negative on failure.
 */
int metac_value_from_cjson(metac_value_t* p_val, struct cJSON* json, metac_tag_map_t* p_tag_map);

#endif // METAC_BACKEND_CJSON_H
