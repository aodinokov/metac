#include "metac/test.h"
#include <stdio.h> /*vsnprintf*/
#include <string.h> /*strncmp*/

#include "metac/backend/va_list_ex.h"


METAC_START_TEST(va_list_ex_sanity) {
    // this doesn't work on Linux
    // /home/runner/work/metac/metac/src/va_list_ex_test.c:16:F:default:va_list_ex_sanity:0: expected 1 2 3 4 5 6 7 8 9 10 got 1 -1 0 4 5 -1267418064 1361077456 1361076992 1353769297 1361077488
    // Windows
    // D:/a/metac/metac/src/va_list_ex_test.c:16:F:default:va_list_ex_sanity:0: expected 1 2 3 4 5 6 7 8 9 10 got 1024 1592742005 -826280184 4 5 6 7 8 9 10
    char buf[1024];
    char *expected_s = NULL;
    expected_s = "1 2 3 4 5 6 7 8 9 10";
    WITH_VA_LIST_CONTAINER(c,
        VA_LIST_CONTAINER(c, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        vsnprintf(buf, sizeof(buf), "%d %d %d %d %d %d %d %d %d %d", c.parameters));
    fail_unless(strncmp(expected_s, buf, sizeof(buf)) == 0, "expected %s got %s", expected_s, buf);

    // so, lets be very careful with stack
    int i, j;
    int fail_i = -1;
    WITH_VA_LIST_CONTAINER(c,
        VA_LIST_CONTAINER(c, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        for (i=1; i<=10; ++i) {
            j = va_arg(c.parameters, int);
            if (j != i) {
                fail_i = i;
                break;
            }
        }
    );
    fail_unless(fail_i < 0, "fail_i: %d", fail_i);

}END_TEST