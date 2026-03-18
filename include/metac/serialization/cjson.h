/**
 * @file cjson.h
 * @brief CJSON- library based serialization/deserialization API for metac
 * 
 * Note: Covers most of the cases, though there is a limitation:
 * in case tag_map is used to identify actual void* type or container_of case
 * it's important to have fields by which this identification happening defined
 * earlier in the structure, so by the time we're identifying parameter of the pointer
 * this data must be already de-serialized.
 * 
 * This module is considered as beta-version of the module.
 */

#ifndef METAC_CJSON_H
#define METAC_CJSON_H

#include "metac/reflect.h" // metac_value_t
#include "metac/serialization/common.h" // metac_value_deserialization_mode_t

#ifdef __cplusplus
extern "C" {
#endif

/* Early declaration of cJSON type */
struct cJSON;

/** @brief Convert a metac_value_t to a cJSON object.
 *  @param p_val The value to convert.
 *  @param wmode The walk mode (shallow or deep).
 *  @param p_tag_map Tagmap to resolve ambiguities.
 *  @return A cJSON object, or NULL on failure.
 */
struct cJSON* metac_value_to_cjson(metac_value_t* p_val, metac_value_walk_mode_t wmode, metac_tag_map_t* p_tag_map);

/** @brief Populate a generic metac_value_t from a cJSON object.
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

/* Couple of lightweight non-recursive functions we can export */
/** @brief Populate a base_type(char, int...complex) metac_value_t from a cJSON object.
 *  @param p_val The destination value to populate.
 *  @param json The source cJSON object.
 *  @return 0 on success, negative on failure.
 */
int metac_value_base_type_from_cjson(metac_value_t* p_val, struct cJSON* json);

/** @brief Populate a base_type(char, int...complex) metac_value_t to a cJSON object.
 *  @param p_val The source value to populate.
 *  @return A cJSON object, or NULL on failure
 */
struct cJSON* metac_value_base_type_to_cjson(metac_value_t* p_val);

/** @brief Populate a enumeration metac_value_t from a cJSON object.
 *  @param p_val The destination value to populate.
 *  @param json The source cJSON object.
 *  @return 0 on success, negative on failure.
 */
int metac_value_enumeration_type_from_cjson(metac_value_t* p_val, struct cJSON* json);

/** @brief Populate a enumeration metac_value_t to a cJSON object.
 *  @param p_val The source value to populate.
 *  @return A cJSON object, or NULL on failure
 */
struct cJSON* metac_value_enumeration_to_cjson(metac_value_t* p_val);

/** @brief Determine if value has flexible part (typically useful for structs) and returns its size in bytes.
 *  @param p_val The destination value to populate.
 *  @param json The source cJSON object.
 *  @param p_mode mode in which this function will work. if NULL - uses default with fail on all unknown situations
 *  @param p_tag_map tag_map is necessary to identify correctly the json members names
 *  @param p_flexible_el_number length of the flexible array in elements.
 *  @param p_flexible_el_sz length of the flexible array 1 element in bytes.
 *  @return 0 on success, negative on failure.
 */
int metac_value_from_cjson_determine_flexible_sz(metac_value_t* p_val, struct cJSON* in_json,
    metac_value_deserialization_mode_t * p_mode,
    metac_tag_map_t* p_tag_map,
    metac_size_t* p_flexible_el_number,
    metac_size_t* p_flexible_el_sz);

#ifdef __cplusplus
}
#endif

#endif // METAC_CJSON_H
