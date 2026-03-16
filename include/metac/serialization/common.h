#include "metac/reflect.h"

/** @brief default behavior in case callback didn't return information for pointer */
typedef enum {
    METAC_DESER_string_ptr_deny = 0,    /**< fail if met string - safest and default*/
    METAC_DESER_string_ptr_allow,       /**< deserialize from string (can be useful just for some very limited use-cases)*/
}metac_value_deser_string_ptr_mode_t;

typedef enum {
    METAC_DESER_array_len_precise = 0,   /**< fail if non-flex array length doesn't match to the array len in json - safest and default*/
    METAC_DESER_array_len_allow_less,    /**< don't fail if json contains less. the remaining will be zero initialized, but fail if json contains more */
    METAC_DESER_array_len_ignore_extra,  /**< don't fail if json contains more, just ingore extra */
}metac_value_deser_array_len_mode_t;

/** @brief mode of deserialization functions */
typedef struct {
    metac_value_deser_string_ptr_mode_t string_ptr_mode; 
    metac_value_deser_array_len_mode_t array_len_mode;
    metac_value_memory_map_uptr_mode_t unknown_ptr_mode;
    metac_value_memory_map_union_mode_t union_mode;
    metac_value_memory_map_flex_array_mode_t flex_array_mode;
}metac_value_deserialization_mode_t;
