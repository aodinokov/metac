#include "metac/test.h"

#include "entry.c"
#include "entry_cdecl.c"
#include "entry_db.c"
#include "entry_tag.c"
#include "iterator.c"
#include "hashmap.c"
#include "value.c"
#include "value_base_type.c"
#include "value_string.c"
#include "value_with_args.c"

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

METAC_START_TEST(test_parse_va_list_per_format_specifier) {
    char _c_ = 100;
    short _s_ = 10000;
    int _i_ = 1000000;
    long _l_ = 20000000;
    long long _ll_ = 20000000;

    struct {
        metac_value_t * p_parsed_value;
        metac_num_t expected_sz;
        char ** expected_s;//strings
    }tcs[] = {
        {
            .p_parsed_value = metac_new_value_printf("%p %p", NULL, NULL),
            .expected_sz = 2,
            .expected_s = (char *[]){"NULL", "NULL"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%p %p", 0x100, 0xff00),
            .expected_sz = 2,
            .expected_s = (char *[]){"0x100", "0xff00"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%c %hhi, %hhd", 'x', 'y', 'z'),
            .expected_sz = 3,
            .expected_s = (char *[]){"'x'", "'y'", "'z'"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%hd %hi", -1500, 1499),
            .expected_sz = 2,
            .expected_s = (char *[]){"-1500", "1499"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%d %i", -100000, 1000001),
            .expected_sz = 2,
            .expected_s = (char *[]){"-100000", "1000001"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%ld %li", -2000000, 2000000),
            .expected_sz = 2,
            .expected_s = (char *[]){"-2000000", "2000000"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%lld %lli", -2000000, 2000000),
            .expected_sz = 2,
            .expected_s = (char *[]){"-2000000", "2000000"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%hho, %hhu, %hhx, %hhX", 118, 120, 121, 122),
            .expected_sz = 4,
            .expected_s = (char *[]){"118", "120", "121", "122"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%ho, %hu, %hx, %hX", 11800, 12000, 12100, 12200),
            .expected_sz = 4,
            .expected_s = (char *[]){"11800", "12000", "12100", "12200"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%o, %u, %x, %X", 1180000, 1200000, 1210000, 1220000),
            .expected_sz = 4,
            .expected_s = (char *[]){"1180000", "1200000", "1210000", "1220000"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%lo, %lu, %lx, %lX", 11800000, 12000000, 12100000, 12200000),
            .expected_sz = 4,
            .expected_s = (char *[]){"11800000", "12000000", "12100000", "12200000"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%llo, %llu, %llx, %llX", 11800000, 12000000, 12100000, 12200000),
            .expected_sz = 4,
            .expected_s = (char *[]){"11800000", "12000000", "12100000", "12200000"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%f, %g, %e", 11.1, 11.2, -11.3),
            .expected_sz = 3,
            .expected_s = (char *[]){"11.100000", "11.200000", "-11.300000"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%Lf, %Lg, %Le", 11.1L, 11.2L, -11.3L),
            .expected_sz = 3,
            .expected_s = (char *[]){"11.100000", "11.200000", "-11.300000"},
        },
        {
            .p_parsed_value = metac_new_value_printf("%hhn, %hn, %n, %ln, %lln", &_c_, &_s_, &_i_, &_l_, &_ll_),
            .expected_sz = 5,
            .expected_s = (char *[]){"(char []){'d',}", "(short int []){10000,}", "(int []){1000000,}", "(long int []){20000000,}", "(long long int []){20000000,}"},
        },

        // {
        //     .p_parsed_value = metac_new_value_printf("%s %s", "some", "test"),
        //     .expected_sz = 2,
        //     .expected_s = (char *[]){"'s', 'o', 'm', 'e'", "'t', 'e', 's', 't'"},
        // },
    };

    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        fail_unless(tcs[tc_inx].p_parsed_value != NULL, "tc %d: parsed_value is null", tc_inx);
        fail_unless(metac_value_load_of_parameter_count(tcs[tc_inx].p_parsed_value) == tcs[tc_inx].expected_sz);

        
        for (int i = 0; i < tcs[tc_inx].expected_sz; ++i) {
            metac_value_t * p = metac_value_load_of_parameter_value(tcs[tc_inx].p_parsed_value, i);
            fail_unless(p != NULL, "tc %d.%d, p is null", tc_inx, i);
            char *s = metac_value_string_ex(p, METAC_WMODE_deep, NULL);
            fail_unless(s != NULL);
            fail_unless(strcmp(tcs[tc_inx].expected_s[i], s) == 0, "tc %d.%d, expected %s, got %s",
                tc_inx, i, tcs[tc_inx].expected_s[i], s);
            printf("%s\n", s);
            free(s);
        }

        metac_value_delete(tcs[tc_inx].p_parsed_value);
    }
}