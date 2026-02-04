#include "metac/serialization/cjson.h"
#include "metac/backend/value.h"
#include <cjson/cJSON.h>

// Forward declaration for recursion
static int metac_value_from_cjson_recursive(metac_value_t* p_val, struct cJSON* json, metac_tag_map_t* p_tag_map);

static int metac_value_base_type_from_cjson(metac_value_t* p_val, struct cJSON* json) {
    if (metac_value_is_bool(p_val)) {
        return metac_value_set_bool(p_val, cJSON_IsTrue(json));
    }
    if (metac_value_is_char(p_val) && cJSON_IsString(json)) {
        return metac_value_set_char(p_val, cJSON_GetStringValue(json)[0]);
    }
    if (cJSON_IsNumber(json)) {
        double num = cJSON_GetNumberValue(json);
        // This is a simplification. A more robust implementation would check the specific integer type.
        if (metac_value_is_float(p_val)) return metac_value_set_float(p_val, (float)num);
        if (metac_value_is_double(p_val)) return metac_value_set_double(p_val, num);
        if (metac_value_is_long(p_val)) return metac_value_set_long(p_val, (long)num);
        if (metac_value_is_int(p_val)) return metac_value_set_int(p_val, (int)num);
        if (metac_value_is_short(p_val)) return metac_value_set_short(p_val, (short)num);
        // Add other numeric types as needed
    }
    return -1; // Type mismatch
}

static int metac_value_from_cjson_recursive(metac_value_t* p_val, struct cJSON* json, metac_tag_map_t* p_tag_map) {
    if (!p_val || !json) return -1;

    metac_kind_t kind = metac_value_final_kind(p_val, NULL);

    switch (kind) {
        case METAC_KND_base_type: {
            return metac_value_base_type_from_cjson(p_val, json);
        }
        case METAC_KND_struct_type: {
            if (!cJSON_IsObject(json)) return -1;

            metac_num_t mcount = metac_value_member_count(p_val);
            for (metac_num_t i = 0; i < mcount; ++i) {
                metac_value_t* p_memb_val = metac_new_value_by_member_id(p_val, i);
                if (!p_memb_val) continue;

                metac_name_t memb_name = metac_value_name(p_memb_val);
                if (!memb_name) {
                    metac_value_delete(p_memb_val);
                    continue;
                }

                struct cJSON* memb_json = cJSON_GetObjectItemCaseSensitive(json, memb_name);
                if (memb_json) {
                    if (metac_value_from_cjson_recursive(p_memb_val, memb_json, p_tag_map) != 0) {
                        // Optional: handle partial deserialization error
                    }
                }
                metac_value_delete(p_memb_val);
            }
            return 0;
        }
        case METAC_KND_array_type: {
            if (!cJSON_IsArray(json)) return -1;

            metac_num_t p_val_count = metac_value_element_count(p_val);
            int json_count = cJSON_GetArraySize(json);
            metac_num_t count = (p_val_count < json_count) ? p_val_count : json_count;

            for (metac_num_t i = 0; i < count; ++i) {
                metac_value_t* p_el_val = metac_new_value_by_element_id(p_val, i);
                if (!p_el_val) continue;

                struct cJSON* el_json = cJSON_GetArrayItem(json, i);
                if (el_json) {
                    if (metac_value_from_cjson_recursive(p_el_val, el_json, p_tag_map) != 0) {
                        // Optional: handle error
                    }
                }
                metac_value_delete(p_el_val);
            }
            return 0;
        }
        // TODO: Handle other kinds
        default:
            return -1; // Unhandled kind
    }
}

int metac_value_from_cjson(metac_value_t* p_val, struct cJSON* json, metac_tag_map_t* p_tag_map) {
    return metac_value_from_cjson_recursive(p_val, json, p_tag_map);
}
