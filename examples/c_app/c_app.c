#include <stdio.h>

#include "metac/reflect.h"
#include "c_app_data.h"

int CONST = 3;
static int STATICCONST=4;

METAC_MODULE(METAC_MODULE_NAME);
METAC_GSYM_LINK(CONST);
METAC_GSYM_LINK(data_sum);
METAC_GSYM_LINK(data_mul);
METAC_DB_LINK();

int main() {
    WITH_METAC_DECLLOC(data_loc, data_t data) = {
        .numbers = {1, 2},
    };
    metac_entry_t * p_data_entry = METAC_ENTRY_FROM_DECLLOC(data_loc, data);

    WITH_METAC_DECLLOC(sum_loc, int sum) = data_sum(&data);
    metac_entry_t * p_sum_entry = METAC_ENTRY_FROM_DECLLOC(sum_loc, sum);
    int mul = data_mul(&data);

    printf("sum: %d, mul: %d, static_const: %d\n", sum, mul, STATICCONST);
    printf("p_data_entry %p, p_sum_entry %p\n", p_data_entry, p_sum_entry);
    return 0;
}