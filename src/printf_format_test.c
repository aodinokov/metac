#include "metac/test.h"

#include "printf_format.c"

METAC_START_TEST(param_count_sanity) {
    struct {
        char * in;
        metac_num_t expected_out;
    }tcs[] = {
        {
            .in = "",
            .expected_out = 0,
        },
        {
            .in = " %% ",
            .expected_out = 0,
        },
        {
            .in = " %% ",
            .expected_out = 0,
        },
        {
            .in = "%% %05p |%12.4f|%12.4e|%12.4g|%12.4Lf|%12.4Lg|\n",
            .expected_out = 6,
        },
    };

    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        metac_num_t out = metac_count_format_specifiers(tcs[tc_inx].in);
        fail_unless(out == tcs[tc_inx].expected_out, 
            "tc_inx %d: expected '%d' got '%d'", tc_inx, tcs[tc_inx].expected_out, out);
    }
}