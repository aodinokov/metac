#include "metac/test.h"
#include <stdio.h>

#include "metac/reflect.h"

// including c-file we want to test, because we need to test some static functions
#include "entry_cdecl.c"
#include "iterator.c"

long int test_0;
METAC_GSYM_LINK(test_0);

enum {
    _e1_a = 1,
    _e1_b = 2,
    _e1_c = 3,
}test_1;
METAC_GSYM_LINK(test_1);

enum eTest2 {
    _et2_1 = -1,
    _et2 = 2,
}test_2;
METAC_GSYM_LINK(test_2);

int test_3[10];
METAC_GSYM_LINK(test_3);

enum {
    _e4_a = 1,
    _e4_b = 2,
    _e4_c = 3,
}test_4[5];
METAC_GSYM_LINK(test_4);

int test_5[][5];
METAC_GSYM_LINK(test_5);

int test_6[][4][5];
METAC_GSYM_LINK(test_6);

const long int test_7 = 0;
METAC_GSYM_LINK(test_7);

const char* test_8 = "test_8"; // ptr to constant chars
METAC_GSYM_LINK(test_8);

char const* test_9 = "test_9"; // ptr to constant chars (the same as 9, but different way to do this). const <type> == <type> const
METAC_GSYM_LINK(test_9);

char * const test_10 = "test_10"; // const ptr to char
METAC_GSYM_LINK(test_10);

const char * const * test_11 = (const char * const[]) { // ptr to const ptr to constant chars
    "1", "2",
};
METAC_GSYM_LINK(test_11);

char * volatile test_12 = "test_12"; // volatile ptr to char
METAC_GSYM_LINK(test_12);

typedef char test_chart_t;
test_chart_t * const test_13 = "test_13"; // const ptr to test_chart_t
METAC_GSYM_LINK(test_13);

struct {
    int a;
    struct {
        double b;
        long long c;
        void * p;
    }x;
}test_14;
METAC_GSYM_LINK(test_14);

struct {
    int a;
    union {
        double b;
        long long c;
        void * p;
    };
}test_15;
METAC_GSYM_LINK(test_15);

struct {
    int a;
    struct {
        int x:5;
        int y:4;
        int z:3;
    }x;
}test_16;
METAC_GSYM_LINK(test_16);

long int (*test_17) (char* in1) = NULL;
METAC_GSYM_LINK(test_17);

long int test_18(char* in1) {
    return 0;
}
METAC_GSYM_LINK(test_18);

void test_19(char* in1, int) { return ;}
METAC_GSYM_LINK(test_19);

int test_20(char* in1, ...) { return 0;}
METAC_GSYM_LINK(test_20);

void * test_21 = NULL;
METAC_GSYM_LINK(test_21);

METAC_START_TEST(test_metac_entry_to_c_decl) {
    struct {
        metac_entry_t * in;
        metac_name_t expected_out;
    }tcs[] = {
        {
            .in = NULL, /* check that NULL is handled properly */
            .expected_out = NULL,
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_0),
            .expected_out = "long int test_0",
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_1),
            .expected_out = "enum {_e1_a = 1, _e1_b = 2, _e1_c = 3,} test_1",
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_2),
            .expected_out = "enum eTest2 test_2",
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_3),
            .expected_out = "int test_3[10]",
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_4),
            .expected_out = "enum {_e4_a = 1, _e4_b = 2, _e4_c = 3,} test_4[5]",
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_5),
#if defined(__clang__)
            .expected_out = "int test_5[1][5]",
#else
            .expected_out = "int test_5[][5]",
#endif
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_6),
#if defined(__clang__)
            .expected_out = "int test_6[1][4][5]",
#else
            .expected_out = "int test_6[][4][5]",
#endif
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_7),
            .expected_out = "const long int test_7",
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_8),
            .expected_out = "const char * test_8",
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_9),
            .expected_out = "const char * test_9",
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_10),
            .expected_out = "char * const test_10",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_11),
            .expected_out = "const char * const * test_11",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_12),
            .expected_out = "char * volatile test_12",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_13),
            .expected_out = "test_chart_t * const test_13",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_14),
            .expected_out = "struct {int a; struct {double b; long long int c; void * p; } x; } test_14",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_15),
            .expected_out = "struct {int a; union {double b; long long int c; void * p; }; } test_15",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_16),
            .expected_out = "struct {int a; struct {int x:5; int y:4; int z:3; } x; } test_16",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_17),
            .expected_out = "long int (* test_17)(char *)",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_18),
            .expected_out = "long int test_18(char * in1)",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_19),
            .expected_out = "void test_19(char * in1, int)",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_20),
            .expected_out = "int test_20(char * in1, ...)",            
        },
        {
            .in = METAC_GSYM_LINK_ENTRY(test_21),
            .expected_out = "void * test_21",            
        },
    };
    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        //printf("-=%d=-\n", tc_inx);
        fail_unless(tcs[tc_inx].expected_out == NULL || tcs[tc_inx].in != NULL, 
            "tc_inx %d: wasn't able to get in p_entry from reflect", tc_inx);
        mark_point();
        char * out = metac_entry_cdecl(tcs[tc_inx].in);
        fail_unless((out != NULL) == (tcs[tc_inx].expected_out != NULL), "tc_inx %d: expected nonNull %d got %d",
            tc_inx, (tcs[tc_inx].expected_out != NULL), (out != NULL));
        if (tcs[tc_inx].expected_out != NULL) {
            fail_unless(strcmp(tcs[tc_inx].expected_out, out) == 0, "tc_inx %d: expected '%s' got '%s'",
                tc_inx, tcs[tc_inx].expected_out, out);
        }
        free(out);
        mark_point();
    }
}