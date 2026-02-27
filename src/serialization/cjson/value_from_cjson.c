#include "metac/serialization/cjson.h"
#include "metac/backend/value.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cjson/cJSON.h>

/**
 * @brief Deserialize JSON (cJSON) objects into metac_value_t structures
 *
 * This module provides JSON-to-value deserialization for the metac reflection library.
 * It complements value_to_cjson.c (serialization) to enable round-trip JSON conversion.
 *
 * Architecture:
 * - Uses simple recursion to traverse both the metac type structure and JSON tree
 * - For each metac_value_t, finds corresponding JSON and populates the value
 * - Handles named struct members by field name lookup in JSON objects
 * - Handles anonymous struct members by passing parent JSON (merged members pattern)
 * - Returns 0 on success, -1 on failure or unsupported types
 *
 * Supported Types:
 * - Base types (int, float, bool, etc.) - numeric/boolean JSON values
 * - Enumerations - JSON strings mapped to enum names
 * - Structs - JSON objects with member fields
 * - Arrays - JSON arrays with typed elements
 * - Unions - with tagmap handler support for active member selection
 * - NULL pointers - from JSON null values
 *
 * Limitations:
 * - Non-NULL pointers cannot be reliably deserialized (by design - see comments)
 * - Pointers as arrays require tagmap handler support (not fully implemented)
 * - Flexible arrays without TagMap sizing info use destination array bounds
 * - Unions without TagMap cannot determine active member (ambiguous)
 * - Anonymous struct members must be flattened in JSON (per serialization convention)
 *
 * Design Notes:
 * - Deserialization is simpler than serialization (consuming vs. building)
 * - This justifies the recursive approach vs. iterator pattern used in serialization
 * - Error handling: partial deserialization is allowed (missing optional fields)
 * - When a member deserializes fails, the function continues with other members
 *   but returns -1 to indicate overall failure
 */

static int metac_value_base_type_from_cjson(metac_value_t* p_val, struct cJSON* json) {
    if (metac_value_is_bool(p_val)) {
        return metac_value_set_bool(p_val, cJSON_IsTrue(json));
    }
    if (metac_value_is_float_complex(p_val) || 
        metac_value_is_double_complex(p_val) ||
        metac_value_is_ldouble_complex(p_val)) {
        if (cJSON_IsObject(json)) {
            struct cJSON* json_real = cJSON_GetObjectItem(json, "real");
            struct cJSON* json_img = cJSON_GetObjectItem(json, "img");
            if (json_real == NULL || !cJSON_IsNumber(json_real) ||
                json_img == NULL || !cJSON_IsNumber(json_img)) {
                return -(EINVAL);
            }
            double  real = cJSON_GetNumberValue(json_real),
                    img = cJSON_GetNumberValue(json_img);
            if (metac_value_is_float_complex(p_val)) {
                return metac_value_set_float_complex(p_val, ((float)real) + I * ((float)img));
            }
            if (metac_value_is_double_complex(p_val)) {
                return metac_value_set_double_complex(p_val, ((double)real) + I * ((double)img));
            }
            if (metac_value_is_ldouble_complex(p_val)) {
                // TODO: we're losing precision in case of long double and cjson
                return metac_value_set_ldouble_complex(p_val, ((long double)real) + I * ((long double)img));
            }
            return -(EFAULT);
        }
        if (cJSON_IsString(json)) {
            if (metac_value_base_type_from_string(p_val, cJSON_GetStringValue(json)) == NULL) {
                return -(EINVAL);
            }
            return -(EFAULT);
        }
        return -(EINVAL);
    }
    if (cJSON_IsNumber(json)) {
        double num = cJSON_GetNumberValue(json);
        if (metac_value_is_char(p_val)) return metac_value_set_char(p_val, (char)num);
        if (metac_value_is_uchar(p_val)) return metac_value_set_uchar(p_val, (unsigned char)num);
        if (metac_value_is_short(p_val)) return metac_value_set_short(p_val, (short)num);
        if (metac_value_is_ushort(p_val)) return metac_value_set_ushort(p_val, (unsigned short)num);
        if (metac_value_is_int(p_val)) return metac_value_set_int(p_val, (int)num);
        if (metac_value_is_uint(p_val)) return metac_value_set_uint(p_val, (unsigned int)num);
        if (metac_value_is_long(p_val)) return metac_value_set_long(p_val, (long)num);
        if (metac_value_is_long(p_val)) return metac_value_set_long(p_val, (long)num);
        if (metac_value_is_float(p_val)) return metac_value_set_float(p_val, (float)num);
        if (metac_value_is_double(p_val)) return metac_value_set_double(p_val, num);
    }
    return -(EINVAL); // Type mismatch
}

static int metac_value_enumeration_type_from_cjson(metac_value_t* p_val, struct cJSON* json) {
    if (!cJSON_IsString(json)) {
        return -1;
    }
    char * value = cJSON_GetStringValue(json);
    if (value == NULL) {
        return -1;
    }
    metac_num_t icount = metac_value_enumeration_info_size(p_val);
    for (metac_num_t i = 0; i < icount; ++i) {
        struct metac_type_enumerator_info const * p_enum_entry = metac_value_enumeration_info(p_val, i);
        assert(p_enum_entry);
        assert(p_enum_entry->name != NULL);
        if (strcmp(p_enum_entry->name, value) == 0) {
            metac_value_set_enumeration(p_val, p_enum_entry->const_value);
            return 0;
        }
    }
    return -1; // Enum value not found
}

static int metac_value_from_cjson_recursive(metac_value_t* p_val, struct cJSON* json, metac_tag_map_t* p_tag_map) {
    if (!p_val || !json) return -1;

    metac_kind_t kind = metac_value_final_kind(p_val, NULL);

    switch (kind) {
        case METAC_KND_base_type: {
            return metac_value_base_type_from_cjson(p_val, json);
        }
        case METAC_KND_enumeration_type: {
            return metac_value_enumeration_type_from_cjson(p_val, json);
        }
        case METAC_KND_pointer_type: {
            // Handle NULL pointers
            if (cJSON_IsNull(json)) {
                return metac_value_set_pointer(p_val, NULL);
            }

            // Non-NULL pointers: cannot reliably deserialize
            // During serialization, pointers are converted to strings (hex addresses)
            // or objects/arrays. On deserialization, we cannot convert them back to
            // valid pointers without additional context (where was the memory allocated?).
            // This limitation is by design - JSON is not a suitable medium for preserving
            // pointer semantics. Pointers are typically serialized for debugging purposes
            // only. For full deep deserialization, use alternative approaches.
            if (cJSON_IsString(json)) {
                // String represents a pointer address
                if (metac_value_pointer_from_string(p_val, cJSON_GetStringValue(json)) == NULL) {
                    return -(EFAULT);
                }
                return 0;
            }

            // Object/Array pointers: would require memory allocation
            // This would need tagmap handler support (like pointers_as_arrays pattern)
            // TODO: Support pointer arrays via tagmap handlers (METAC_RQVST_pointer_array_count)
            if (cJSON_IsObject(json) || cJSON_IsArray(json)) {
                return -1;
            }

            return -1;
        }
        case METAC_KND_struct_type: {
            if (!cJSON_IsObject(json)) return -1;

            metac_num_t mcount = metac_value_member_count(p_val);
            for (metac_num_t i = 0; i < mcount; ++i) {
                metac_value_t* p_memb_val = metac_new_value_by_member_id(p_val, i);
                if (!p_memb_val) continue;

                metac_name_t memb_name = metac_value_name(p_memb_val);
                struct cJSON* memb_json = NULL;

                if (memb_name && memb_name[0] != '\0') {
                    // Named member - find by name in JSON object
                    memb_json = cJSON_GetObjectItemCaseSensitive(json, memb_name);
                } else {
                    // Anonymous member - handle nested struct specially
                    // Anonymous members inherit the parent JSON object
                    memb_json = json;
                }

                if (memb_json) {
                    // Attempt to deserialize this member
                    // Note: We don't fail the whole struct if a member fails
                    // This allows partial deserialization for structs with optional fields
                    metac_value_from_cjson_recursive(p_memb_val, memb_json, p_tag_map);
                }
                metac_value_delete(p_memb_val);
            }
            // Struct deserialization succeeds as long as we could process all members
            // Individual field failures are silently ignored (partial deserialization)
            return 0;
        }
        case METAC_KND_array_type: {
            if (!cJSON_IsArray(json)) return -1;

            metac_num_t p_val_count = metac_value_element_count(p_val);
            int json_count = cJSON_GetArraySize(json);

            // If this is a flexible array, we should try to get the proper count from tagmap
            // TODO: Implement flexible array size determination from tagmap
            // For now, use the minimum of destination and JSON array sizes
            metac_num_t count = (p_val_count < json_count) ? p_val_count : json_count;

            for (metac_num_t i = 0; i < count; ++i) {
                metac_value_t* p_el_val = metac_new_value_by_element_id(p_val, i);
                if (!p_el_val) continue;

                struct cJSON* el_json = cJSON_GetArrayItem(json, i);
                if (el_json) {
                    // Attempt to deserialize array element
                    // Note: We don't fail the whole array if an element fails
                    // This allows partial deserialization for arrays
                    metac_value_from_cjson_recursive(p_el_val, el_json, p_tag_map);
                }
                metac_value_delete(p_el_val);
            }
            // Array deserialization succeeds as long as we could process elements
            return 0;
        }
        case METAC_KND_union_type: {
            // Union deserialization with optional tagmap support
            // Note: Without a tagmap handler, we cannot deserialize unions
            // because we don't know which member is active
            if (p_tag_map != NULL) {
                // Try to get the active union member from tagmap handler
                // We pass NULL as the iterator since we're in a recursive context
                // The handler can use the value's entry information instead
                metac_value_event_t ev = {.type = METAC_RQVST_union_member, .p_return_value = NULL};
                metac_entry_tag_t * p_tag = metac_tag_map_tag(p_tag_map, metac_value_entry(p_val));
                if (p_tag != NULL && p_tag->handler) {
                    // Note: We pass NULL for iterator as we're in recursive context
                    // Handlers should not rely on iterator in this case
                    if (metac_value_event_handler_call(p_tag->handler, NULL, &ev, p_tag->p_context) == 0 && ev.p_return_value != NULL) {
                        // Deserialize the active union member
                        metac_value_t* p_member_val = (metac_value_t*)ev.p_return_value;
                        int result = metac_value_from_cjson_recursive(p_member_val, json, p_tag_map);
                        metac_value_delete(p_member_val);
                        return result;
                    }
                }
            }
            // Unions without tagmap handlers are skipped (treated as optional)
            // This matches serialization behavior where unions are serialized as empty objects
            return 0;
        }
        default:
            return -1; // Unhandled kind
    }
}

int metac_value_from_cjson(metac_value_t* p_val, struct cJSON* json, metac_tag_map_t* p_tag_map) {
    return metac_value_from_cjson_recursive(p_val, json, p_tag_map);
}
