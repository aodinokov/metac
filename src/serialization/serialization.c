#include "metac/backend/serialization.h"

#include <stdlib.h>

metac_deserialization_pair_t * metac_new_deserialization_pair(metac_value_t * p_val, void * p_external) {
    metac_deserialization_pair_t * p_pair = calloc(1, sizeof(*p_pair));
    if (p_pair == NULL) {
        return NULL;
    }
    p_pair->p_val = p_val;
    p_pair->p_external = p_external;
    return p_pair;
}

void metac_deserialization_pair_delete(metac_deserialization_pair_t * p_pair) {
    if (p_pair == NULL) {
        return;
    }
    free(p_pair);
}
