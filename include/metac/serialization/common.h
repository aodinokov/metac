#include "metac/reflect.h"

/** @brief mode of deserialization functions */
typedef struct {
    metac_value_memory_map_uptr_mode_t unknown_ptr_mode;
    metac_value_memory_map_union_mode_t union_mode;
    metac_value_memory_map_flex_array_mode_t flex_array_mode;
}metac_value_deserialization_mode_t;
