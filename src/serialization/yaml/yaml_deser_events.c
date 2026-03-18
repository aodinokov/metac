/**
 * @file yaml_deser_events.c
 * @brief Core YAML deserialization algorithm
 *
 * Implements the main deserial ization logic:
 * - from_string(): Read YAML and populate metac_value_t
 * - Event processing: Recursive event handlers for different value types
 * - Type conversion: String-to-value conversion functions
 *
 * This is SHARED CODE used by both Context API and Convenience API.
 */

#include "metac/serialization/yaml.h"
#include "metac/reflect.h"
#include <stdlib.h>
#include <string.h>
#include <yaml.h>
#include <stdio.h>

/*
 * Forward declarations
 */
static int yaml_event_to_value(
    metac_yaml_deserialization_t *p_deser,
    metac_value_t *p_target,
    yaml_event_t *p_event);

static int yaml_parse_scalar_to_value(
    metac_yaml_deserialization_t *p_deser,
    metac_value_t *p_target,
    const char *scalar_value);

/**
 * @brief Parse scalar string to enumeration value
 *
 * Converts a YAML scalar string (enum name) to the appropriate enum value.
 *
 * @param p_deser       Deserialization context
 * @param p_target      Target enumeration value
 * @param enum_name     Enum name string from YAML
 *
 * @return 0 on success, -1 on error
 */
static int yaml_parse_enum(
    metac_yaml_deserialization_t *p_deser,
    metac_value_t *p_target,
    const char *enum_name)
{
    if (!p_target || !enum_name) {
        return -1;
    }

    // Get the enum entry
    metac_entry_t *p_entry = metac_value_entry(p_target);
    if (!p_entry) {
        metac_yaml_deserialization_set_error(p_deser, "Failed to get enum entry", -1);
        return -1;
    }

    // Iterate through all enumeration values
    metac_num_t enum_count = metac_entry_enumeration_info_size(p_entry);
    for (metac_num_t i = 0; i < enum_count; ++i) {
        struct metac_type_enumerator_info const *p_enum_info =
            metac_entry_enumeration_info(p_entry, i);

        if (p_enum_info && p_enum_info->name && strcmp(p_enum_info->name, enum_name) == 0) {
            // Found matching enum name - set the value
            int ret = metac_value_set_enumeration(p_target, p_enum_info->const_value);
            if (ret != 0) {
                metac_yaml_deserialization_set_error(p_deser,
                    "Failed to set enumeration value", -1);
                return -1;
            }
            return 0;
        }
    }

    // Enum value not found
    metac_yaml_deserialization_set_error(p_deser,
        "Unknown enumeration value", -1);
    return -1;
}

/**
 * @brief Parse scalar string to base type value
 *
 * Converts a YAML scalar string to the appropriate C type based on the target value's type.
 * Strict validation: type mismatch returns error.
 *
 * @param p_deser       Deserialization context (for error reporting)
 * @param p_target      Target metac_value_t (provides type information)
 * @param scalar_value  String from YAML
 *
 * @return 0 on success, -1 on error
 */
static int yaml_parse_scalar_to_value(
    metac_yaml_deserialization_t *p_deser,
    metac_value_t *p_target,
    const char *scalar_value)
{
    if (!p_target || !scalar_value) {
        return -1;
    }

    // Check for enumeration first
    if (metac_value_is_enumeration(p_target)) {
        // Parse enum by name
        return yaml_parse_enum(p_deser, p_target, scalar_value);
    }

    // Check for base types
    if (metac_value_is_base_type(p_target)) {
        // Parse based on the actual type
        int ret = -1;

        if (metac_value_is_int(p_target)) {
            int val = (int)strtol(scalar_value, NULL, 10);
            ret = metac_value_set_int(p_target, val);
        } else if (metac_value_is_float(p_target)) {
            float val = (float)strtod(scalar_value, NULL);
            ret = metac_value_set_float(p_target, val);
        } else if (metac_value_is_double(p_target)) {
            double val = strtod(scalar_value, NULL);
            ret = metac_value_set_double(p_target, val);
        } else if (metac_value_is_char(p_target)) {
            // Char values are serialized as 'X' (with quotes) for printable chars
            // or as numeric values for non-printable chars
            char val = 0;
            if (scalar_value[0] == '\'' && scalar_value[2] == '\'' && scalar_value[3] == '\0') {
                // Format: 'X' where X is the character
                val = scalar_value[1];
            } else {
                // Format: numeric value or just the character itself
                val = (scalar_value[0] != '\0') ? scalar_value[0] : 0;
            }
            ret = metac_value_set_char(p_target, val);
        } else {
            // For other base types, attempt generic approach
            size_t str_len = strlen(scalar_value);
            ret = metac_value_set_base_type(p_target, NULL, 0, (void *)scalar_value, str_len);
        }

        if (ret != 0) {
            metac_yaml_deserialization_set_error(p_deser,
                "Failed to set base type value from scalar", -1);
        }
        return ret;
    }

    // For other types, error
    metac_yaml_deserialization_set_error(p_deser,
        "Scalar type mismatch: expected scalar value", -1);
    return -1;
}

/**
 * @brief Process a YAML mapping into a struct/composite value
 *
 * Iteratively processes key-value pairs from the YAML mapping and assigns
 * them to corresponding struct members.
 *
 * @param p_deser       Deserialization context
 * @param p_parser      YAML parser
 * @param p_target      Target struct value
 *
 * @return 0 on success, -1 on error
 */
static int yaml_parse_mapping(
    metac_yaml_deserialization_t *p_deser,
    yaml_parser_t *p_parser,
    metac_value_t *p_target)
{
    if (!metac_value_has_members(p_target)) {
        metac_yaml_deserialization_set_error(p_deser,
            "Expected struct/composite type but got scalar", -1);
        return -1;
    }

    yaml_event_t event;
    memset(&event, 0, sizeof(event));

    while (1) {
        // Get next event
        if (!yaml_parser_parse(p_parser, &event)) {
            metac_yaml_deserialization_set_error(p_deser,
                "Parser error in mapping", -1);
            yaml_event_delete(&event);
            return -1;
        }

        // Check for mapping end
        if (event.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&event);
            return 0;
        }

        // Expect key scalar
        if (event.type != YAML_SCALAR_EVENT) {
            metac_yaml_deserialization_set_error(p_deser,
                "Expected key scalar in mapping", -1);
            yaml_event_delete(&event);
            return -1;
        }

        // Get member name - need to ensure null termination
        const yaml_char_t *key_value = event.data.scalar.value;
        size_t key_length = event.data.scalar.length;

        // Safely copy key to null-terminated string
        char key_name[256];
        if (key_length >= sizeof(key_name)) {
            metac_yaml_deserialization_set_error(p_deser,
                "Member name too long", -1);
            yaml_event_delete(&event);
            return -1;
        }
        memcpy(key_name, key_value, key_length);
        key_name[key_length] = '\0';

        // Find corresponding member in target
        metac_num_t member_count = metac_value_member_count(p_target);
        metac_value_t *p_member = NULL;
        for (metac_num_t i = 0; i < member_count; ++i) {
            metac_value_t *p_m = metac_new_value_by_member_id(p_target, i);
            if (p_m) {
                metac_name_t m_name = metac_value_name(p_m);
                if (m_name && strcmp(m_name, key_name) == 0) {
                    p_member = p_m;
                    break;
                }
                metac_value_delete(p_m);
            }
        }

        if (!p_member) {
            metac_yaml_deserialization_set_error(p_deser,
                "Unknown struct member", -1);
            yaml_event_delete(&event);
            return -1;
        }

        yaml_event_delete(&event);

        // Get next event (value)
        memset(&event, 0, sizeof(event));
        if (!yaml_parser_parse(p_parser, &event)) {
            metac_yaml_deserialization_set_error(p_deser,
                "Parser error getting value", -1);
            metac_value_delete(p_member);
            yaml_event_delete(&event);
            return -1;
        }

        // Process value recursively
        if (yaml_event_to_value(p_deser, p_member, &event) != 0) {
            metac_value_delete(p_member);
            yaml_event_delete(&event);
            return -1;
        }

        metac_value_delete(p_member);
        yaml_event_delete(&event);
    }

    return 0;
}

/**
 * @brief Process a YAML sequence into an array value
 *
 * Iteratively processes elements from the YAML sequence and assigns
 * them to array elements.
 *
 * @param p_deser       Deserialization context
 * @param p_parser      YAML parser
 * @param p_target      Target array value
 *
 * @return 0 on success, -1 on error
 */
static int yaml_parse_sequence(
    metac_yaml_deserialization_t *p_deser,
    yaml_parser_t *p_parser,
    metac_value_t *p_target)
{
    if (!metac_value_has_elements(p_target)) {
        metac_yaml_deserialization_set_error(p_deser,
            "Expected array type", -1);
        return -1;
    }

    metac_num_t element_count = metac_value_element_count(p_target);
    if (element_count < 0) {
        metac_yaml_deserialization_set_error(p_deser,
            "Array with unknown element count cannot be deserialized", -1);
        return -1;
    }

    yaml_event_t event;
    memset(&event, 0, sizeof(event));
    metac_num_t index = 0;

    while (1) {
        // Get next event
        if (!yaml_parser_parse(p_parser, &event)) {
            metac_yaml_deserialization_set_error(p_deser,
                "Parser error in sequence", -1);
            yaml_event_delete(&event);
            return -1;
        }

        // Check for sequence end
        if (event.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&event);
            if (index != element_count) {
                metac_yaml_deserialization_set_error(p_deser,
                    "Array element count mismatch", -1);
                return -1;
            }
            return 0;
        }

        // Check for too many elements
        if (index >= element_count) {
            metac_yaml_deserialization_set_error(p_deser,
                "Too many elements in sequence", -1);
            yaml_event_delete(&event);
            return -1;
        }

        // Get element value
        metac_value_t *p_elem = metac_new_value_by_element_id(p_target, index);
        if (!p_elem) {
            metac_yaml_deserialization_set_error(p_deser,
                "Failed to create element value", -1);
            yaml_event_delete(&event);
            return -1;
        }

        // Process element recursively
        if (yaml_event_to_value(p_deser, p_elem, &event) != 0) {
            metac_value_delete(p_elem);
            yaml_event_delete(&event);
            return -1;
        }

        metac_value_delete(p_elem);
        yaml_event_delete(&event);
        index++;
    }

    return 0;
}

/**
 * @brief Process a YAML event and assign to target value
 *
 * Recursive function that handles different YAML event types and
 * assigns values to the target metac_value_t.
 *
 * @param p_deser   Deserialization context
 * @param p_target  Target metac_value_t
 * @param p_event   Current YAML event - owns the lifetime
 *
 * @return 0 on success, -1 on error
 */
static int yaml_event_to_value(
    metac_yaml_deserialization_t *p_deser,
    metac_value_t *p_target,
    yaml_event_t *p_event)
{
    if (!p_deser || !p_target || !p_event) {
        return -1;
    }

    switch (p_event->type) {
        case YAML_SCALAR_EVENT: {
            // Parse scalar to value - need to use length field for safety
            const yaml_char_t *scalar_val = p_event->data.scalar.value;
            size_t scalar_len = p_event->data.scalar.length;

            // Create null-terminated string for parsing
            char scalar_str[1024];
            if (scalar_len >= sizeof(scalar_str)) {
                metac_yaml_deserialization_set_error(p_deser,
                    "Scalar value too long", -1);
                return -1;
            }
            memcpy(scalar_str, scalar_val, scalar_len);
            scalar_str[scalar_len] = '\0';

            return yaml_parse_scalar_to_value(p_deser, p_target, scalar_str);
        }

        case YAML_MAPPING_START_EVENT:
            // Parse mapping (struct/composite)
            return yaml_parse_mapping(p_deser, &p_deser->parser, p_target);

        case YAML_SEQUENCE_START_EVENT:
            // Parse sequence (array)
            return yaml_parse_sequence(p_deser, &p_deser->parser, p_target);

        default:
            metac_yaml_deserialization_set_error(p_deser,
                "Unexpected YAML event type", -1);
            return -1;
    }
}

/**
 * @brief Parse YAML string and populate metac_value_t
 *
 * Reads a complete YAML string and deserializes it into the target value.
 *
 * Algorithm:
 * 1. Set parser input to YAML string
 * 2. Read and validate stream/document start events
 * 3. Process root value event recursively
 * 4. Validate stream/document end events
 * 5. Return success or error
 *
 * @param p_deser       Deserialization context
 * @param p_yaml_string YAML string to parse
 * @param yaml_len      Length of YAML string
 *
 * @return 0 on success, -1 on failure
 */
int metac_yaml_deserialization_from_string(
    metac_yaml_deserialization_t *p_deser,
    const char *p_yaml_string,
    size_t yaml_len)
{
    if (!p_deser || !p_yaml_string) {
        return -1;
    }

    // Set parser input
    yaml_parser_set_input_string(&p_deser->parser,
        (const unsigned char *)p_yaml_string, yaml_len);

    yaml_event_t event;
    int ret = 0;

    // Get stream start event
    memset(&event, 0, sizeof(event));
    if (!yaml_parser_parse(&p_deser->parser, &event)) {
        metac_yaml_deserialization_set_error(p_deser,
            "Failed to parse stream start", -1);
        return -1;
    }
    if (event.type != YAML_STREAM_START_EVENT) {
        metac_yaml_deserialization_set_error(p_deser,
            "Expected stream start event", -1);
        yaml_event_delete(&event);
        return -1;
    }
    yaml_event_delete(&event);

    // Get document start event
    memset(&event, 0, sizeof(event));
    if (!yaml_parser_parse(&p_deser->parser, &event)) {
        metac_yaml_deserialization_set_error(p_deser,
            "Failed to parse document start", -1);
        return -1;
    }
    if (event.type != YAML_DOCUMENT_START_EVENT) {
        metac_yaml_deserialization_set_error(p_deser,
            "Expected document start event", -1);
        yaml_event_delete(&event);
        return -1;
    }
    yaml_event_delete(&event);

    // Get and process root value event
    memset(&event, 0, sizeof(event));
    if (!yaml_parser_parse(&p_deser->parser, &event)) {
        metac_yaml_deserialization_set_error(p_deser,
            "Failed to parse root value", -1);
        return -1;
    }

    ret = yaml_event_to_value(p_deser, p_deser->p_value, &event);
    yaml_event_delete(&event);

    if (ret != 0) {
        return -1;  // Error already set by yaml_event_to_value
    }

    // Get document end event
    memset(&event, 0, sizeof(event));
    if (!yaml_parser_parse(&p_deser->parser, &event)) {
        metac_yaml_deserialization_set_error(p_deser,
            "Failed to parse document end", -1);
        return -1;
    }
    if (event.type != YAML_DOCUMENT_END_EVENT) {
        metac_yaml_deserialization_set_error(p_deser,
            "Expected document end event", -1);
        yaml_event_delete(&event);
        return -1;
    }
    yaml_event_delete(&event);

    // Get stream end event
    memset(&event, 0, sizeof(event));
    if (!yaml_parser_parse(&p_deser->parser, &event)) {
        metac_yaml_deserialization_set_error(p_deser,
            "Failed to parse stream end", -1);
        return -1;
    }
    if (event.type != YAML_STREAM_END_EVENT) {
        metac_yaml_deserialization_set_error(p_deser,
            "Expected stream end event", -1);
        yaml_event_delete(&event);
        return -1;
    }
    yaml_event_delete(&event);

    p_deser->parsed = 1;
    return 0;
}
