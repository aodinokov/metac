#include "metac/test.h"

#include "metac/reflect.h"
// this approach allows to check static functions
#include "c_app_data.c"

#include <stdlib.h> /*free*/


METAC_MODULE(METAC_MODULE_NAME);
WITH_METAC_DECLLOC(glob_test, static int glob_test) = 1;

WITH_METAC_DECLLOC(test_fn, long long int test_fn(int a)) {
    return 1;
}

METAC_START_TEST(test_some_thing) {
    WITH_METAC_DECLLOC(data, data_t data) = {
        .numbers = {1, 2},
    };
    char *p_data_cdecl = metac_entry_cdecl(METAC_ENTRY_FROM_DECLLOC(data, data));
    fail_unless(p_data_cdecl!= NULL);
    fail_unless(strcmp(p_data_cdecl, "data_t data") == 0, "unexpected data cdecl %s", p_data_cdecl);
    printf("cdecl: %s\n", p_data_cdecl);
    free(p_data_cdecl);

    metac_entry_t * p_glob_test_entry = METAC_ENTRY_FROM_DECLLOC(glob_test, glob_test);
    fail_unless(p_glob_test_entry != NULL, "wasn't able to get entry for glob_test");

    char *p_glob_test_cdecl = metac_entry_cdecl(METAC_ENTRY_FROM_DECLLOC(glob_test, glob_test));
    fail_unless(p_glob_test_cdecl!= NULL);
    fail_unless(strcmp(p_glob_test_cdecl, "int glob_test") == 0, "unexpected p_glob_test_cdecl %s", p_glob_test_cdecl);
    printf("cdecl: %s\n", p_glob_test_cdecl);
    free(p_glob_test_cdecl);

    char * p_test_fn_cdecl = metac_entry_cdecl(METAC_ENTRY_FROM_DECLLOC(test_fn, test_fn));
    fail_unless(p_test_fn_cdecl!= NULL);
    fail_unless(strcmp(p_test_fn_cdecl, "long long int test_fn(int a)") == 0, "unexpected test_fn cdecl %s", p_test_fn_cdecl);
    printf("cdecl: %s\n", p_test_fn_cdecl);
    free(p_test_fn_cdecl);

    fail_unless(glob_test == 1, "just to use static var so clang could generate debug info");

    /*
    WITH_METAC_ENTRY(data_t, data) = {
        .numbers = {1, 2},
    };
    metac_interface_t i = WITH_METAC_ENTRY_interface(interface_type,data);// -> will expand to new_interface(interface_type,METAC_ETNRY(data), &data};
    
    concept of interface isn't clear yet. this is too much work.
    let's make a shortcut for now

    WITH_METAC_ENTRY(data_t, data) = {
        .numbers = {1, 2},
    };
    metac_value_t v = WITH_METAC_ENTRY_value_of(data); // will create value
    */

    int sum = data_sum(&data);
    int mul = data_mul(&data);

    fail_unless(sum == 3, "got sum %d, expected 3", sum);
    fail_unless(mul == 2, "got mul %d, expected 2", sum);

}END_TEST

