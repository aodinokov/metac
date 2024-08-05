#include "metac/test.h"
#include <stdio.h> /*vsnprintf*/
#include <string.h> /*strncmp*/

#include "metac/backend/va_list_ex.h"

METAC_START_TEST(va_list_ex_precheck) {
    // the original test which worked even without alloca protection
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

METAC_START_TEST(va_list_ex_sanity) {
    char buf[1024];
    char *expected_s = NULL;

    expected_s = "1 2 3 4 5 6 7 8 9 10";
    WITH_VA_LIST_CONTAINER(c,
        vsnprintf(buf, sizeof(buf), "%d %d %d %d %d %d %d %d %d %d", VA_LIST_FROM_CONTAINER(c, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
    );
    fail_unless(strncmp(expected_s, buf, sizeof(buf)) == 0, "expected %s got %s", expected_s, buf);

    WITH_VA_LIST_CONTAINER(c,
        vsnprintf(buf, sizeof(buf), "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", VA_LIST_FROM_CONTAINER(c, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 9L, 10L));
    );
    fail_unless(strncmp(expected_s, buf, sizeof(buf)) == 0, "expected %s got %s", expected_s, buf);

    WITH_VA_LIST_CONTAINER(c,
        vsnprintf(buf, sizeof(buf), "%s %s %s %s %s %s %s %s %s %s", VA_LIST_FROM_CONTAINER(c, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"));
    );
    fail_unless(strncmp(expected_s, buf, sizeof(buf)) == 0, "expected %s got %s", expected_s, buf);

    expected_s = "1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0";
    WITH_VA_LIST_CONTAINER(c,
        vsnprintf(buf, sizeof(buf), "%.1Lf %.1Lf %.1Lf %.1Lf %.1Lf %.1Lf %.1Lf %.1Lf %.1Lf %.1Lf", VA_LIST_FROM_CONTAINER(c, 1.0L, 2.0L, 3.0L, 4.0L, 5.0L, 6.0L, 7.0L, 8.0L, 9.0L, 10.0L));
    );
    fail_unless(strncmp(expected_s, buf, sizeof(buf)) == 0, "expected %s got %s", expected_s, buf);

}END_TEST