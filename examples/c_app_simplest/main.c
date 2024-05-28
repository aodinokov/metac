#include <stdio.h>  // printf
#include <stdlib.h> // free
#include <math.h>   // M_PI, M_E
#include "metac/reflect.h"

struct test {
    int y;
    char c;
    double pi;
    double e;
    short _uninitialized_field;
};

int main(){
    // we need to use this construction to wrap variable declaration
    // to get its type information
    WITH_METAC_DECLLOC(decl_location,
        struct test t = {
            .y = -10,
            .c = 'a',
            .pi = M_PI,
            .e = M_E,
        };
    )
    metac_value_t *p_val = METAC_VALUE_FROM_DECLLOC(decl_location, t);

    char * s;
    s = metac_entry_cdecl(metac_value_entry(p_val));
    // next will output "struct test t = "
    printf("%s = ", s);
    free(s);

    s = metac_value_string(p_val);
    // next will output "{.y = -10, .c = 'a', .pi = 3.141593, .e = 2.718282, ._uninitialized_field = 0,};\n"
    printf("%s;\n", s);
    free(s);

    metac_value_delete(p_val);

    return 0;
}
