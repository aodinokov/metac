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

#define STORE(_args_...) ({ \
    metac_value_t * p_val = NULL; \
    metac_parameter_storage_t * p_param_storage = metac_new_parameter_storage(); \
    if (p_param_storage != NULL) { \
        if (metac_store_printf_params(p_param_storage, _args_) != 0) { \
            metac_parameter_storage_delete(p_param_storage); \
        } else { \
            p_val = metac_new_value(metac_store_printf_params_entry(), p_param_storage); \
        } \
    } \
    p_val; \
})

METAC_START_TEST(test_parse_va_list_per_format_specifier) {
    char _c_ = 100;
    short _s_ = 10000;
    int _i_ = 1000000;
    long _l_ = 20000000;
    long long _ll_ = 20000000;

    char b1[32], b2[32];
    snprintf(b1, sizeof(b1), "%p", (void*)0x100);
    snprintf(b2, sizeof(b2), "%p", (void*)0xff00);

    struct {
        metac_value_t * p_parsed_value;
        metac_num_t expected_sz;
        char ** expected_s;//strings
    }tcs[] = {
        {
            .p_parsed_value = STORE("%p %p", NULL, NULL),
            .expected_sz = 2,
            .expected_s = (char *[]){"NULL", "NULL"},
        },
        {
            .p_parsed_value = STORE("%p %p", (void*)0x100,(void*)0xff00),
            .expected_sz = 2,
            .expected_s = (char *[]){b1, b2},
        },
        {
            .p_parsed_value = STORE("%c %hhi, %hhd", 'x', 'y', 'z'),
            .expected_sz = 3,
            .expected_s = (char *[]){"'x'", "'y'", "'z'"},
        },
        {
            .p_parsed_value = STORE("%hd %hi", -1500, 1499),
            .expected_sz = 2,
            .expected_s = (char *[]){"-1500", "1499"},
        },
        {
            .p_parsed_value = STORE("%d %i", -100000, 1000001),
            .expected_sz = 2,
            .expected_s = (char *[]){"-100000", "1000001"},
        },
        {
            .p_parsed_value = STORE("%ld %li", -2000000L, 2000000L),
            .expected_sz = 2,
            .expected_s = (char *[]){"-2000000", "2000000"},
        },
        {
            .p_parsed_value = STORE("%lld %lli", -2000000LL, 2000000LL),
            .expected_sz = 2,
            .expected_s = (char *[]){"-2000000", "2000000"},
        },
        {
            .p_parsed_value = STORE("%hho, %hhu, %hhx, %hhX", 118, 120, 121, 122),
            .expected_sz = 4,
            .expected_s = (char *[]){"118", "120", "121", "122"},
        },
        {
            .p_parsed_value = STORE("%ho, %hu, %hx, %hX", 11800, 12000, 12100, 12200),
            .expected_sz = 4,
            .expected_s = (char *[]){"11800", "12000", "12100", "12200"},
        },
        {
            .p_parsed_value = STORE("%o, %u, %x, %X", 1180000, 1200000, 1210000, 1220000),
            .expected_sz = 4,
            .expected_s = (char *[]){"1180000", "1200000", "1210000", "1220000"},
        },
        {
            .p_parsed_value = STORE("%lo, %lu, %lx, %lX", 11800000, 12000000, 12100000, 12200000),
            .expected_sz = 4,
            .expected_s = (char *[]){"11800000", "12000000", "12100000", "12200000"},
        },
        {
            .p_parsed_value = STORE("%llo, %llu, %llx, %llX", 11800000, 12000000, 12100000, 12200000),
            .expected_sz = 4,
            .expected_s = (char *[]){"11800000", "12000000", "12100000", "12200000"},
        },
        {
            .p_parsed_value = STORE("%f, %g, %e", 11.1, 11.2, -11.3),
            .expected_sz = 3,
            .expected_s = (char *[]){"11.100000", "11.200000", "-11.300000"},
        },
        {
            .p_parsed_value = STORE("%Lf, %Lg, %Le", 11.1L, 11.2L, -11.3L),
            .expected_sz = 3,
            .expected_s = (char *[]){"11.100000", "11.200000", "-11.300000"},
        },
        {
            .p_parsed_value = STORE("%hhn, %hn, %n, %ln, %lln", &_c_, &_s_, &_i_, &_l_, &_ll_),
            .expected_sz = 5,
            .expected_s = (char *[]){"(char []){'d',}", "(short int []){10000,}", "(int []){1000000,}", "(long int []){20000000,}", "(long long int []){20000000,}"},
        },
        {
            .p_parsed_value = STORE("%s %s", "some", "test"),
            .expected_sz = 2,
            .expected_s = (char *[]){"{'s', 'o', 'm', 'e', 0,}", "{'t', 'e', 's', 't', 0,}"},
        },
        {
            .p_parsed_value = STORE("%s %s", NULL, NULL),
            .expected_sz = 2,
            .expected_s = (char *[]){"NULL", "NULL"},
        },
    };

    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        fail_unless(tcs[tc_inx].p_parsed_value != NULL, "tc %d: parsed_value is null", tc_inx);
        fail_unless(metac_value_parameter_count(tcs[tc_inx].p_parsed_value) == tcs[tc_inx].expected_sz);

        
        for (int i = 0; i < tcs[tc_inx].expected_sz; ++i) {
            metac_value_t * p = metac_value_parameter_new_item(tcs[tc_inx].p_parsed_value, i);
            fail_unless(p != NULL, "tc %d.%d, p is null", tc_inx, i);
            char *s = metac_value_string_ex(p, METAC_WMODE_deep, NULL);
            metac_value_delete(p);
            fail_unless(s != NULL);
            fail_unless(strcmp(tcs[tc_inx].expected_s[i], s) == 0, "tc %d.%d, expected %s, got %s",
                tc_inx, i, tcs[tc_inx].expected_s[i], s);
            free(s);
        }

        metac_parameter_storage_t * p_param_storage = (metac_parameter_storage_t *)metac_value_addr(tcs[tc_inx].p_parsed_value);
        if (p_param_storage != NULL) {
            metac_parameter_storage_delete(p_param_storage);
        }
        metac_value_delete(tcs[tc_inx].p_parsed_value);
    }
}