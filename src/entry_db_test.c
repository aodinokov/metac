#include "metac/test.h"

// including c-file we want to test, because we need to test some static functions
#include "entry_db.c"
// need this to test everythin
#include "entry_cdecl.c"
#include "iterator.c"

#define ARGS_FOR_METAC_ENTRY_FROM_DECLLOC(_name_, _var_) \
        .p_entry_db = METAC_ENTRY_DB, \
        .p_decl_loc = METAC_DECLLOC_PTR(_name_), \
        .varname = #_var_

WITH_METAC_DECLLOC(fn_data_loc1, int test_fn1(){
    int x = 1;
    return 0;
})

WITH_METAC_DECLLOC(gl_data_loc1, int gl_x = 1, gl_y =2);

METAC_START_TEST(test_metac_entry_from_entry_db) {
    WITH_METAC_DECLLOC(local_data_loc, int local_data) = 1;
    WITH_METAC_DECLLOC(local_data_loc1,) int locat_data3 = 1; { WITH_METAC_DECLLOC(local_data_loc2, char local_data1) = 2;
        struct tc_metac_entry_from_entry_db {
            struct metac_entry_db * p_entry_db;
            struct metac_decl_loc * p_decl_loc;
            const void * p_data;
            const metac_name_t varname;
            metac_name_t expected_cdecl;
        }tcs[] = {
            {
                ARGS_FOR_METAC_ENTRY_FROM_DECLLOC(local_data_loc, local_data),
                .expected_cdecl = "int local_data",
            },
            {
                ARGS_FOR_METAC_ENTRY_FROM_DECLLOC(local_data_loc1, locat_data3),
                .expected_cdecl = "int locat_data3",
            },
            {
                ARGS_FOR_METAC_ENTRY_FROM_DECLLOC(local_data_loc2, local_data1),
                .expected_cdecl = "char local_data1",
            },
            {
                ARGS_FOR_METAC_ENTRY_FROM_DECLLOC(fn_data_loc1, test_fn1),
                .expected_cdecl = "int test_fn1()",
            },
            {
                ARGS_FOR_METAC_ENTRY_FROM_DECLLOC(gl_data_loc1, gl_x),
                .expected_cdecl = "int gl_x",
            },
            {
                ARGS_FOR_METAC_ENTRY_FROM_DECLLOC(gl_data_loc1, gl_y),
                .expected_cdecl = "int gl_y",
            },
        };
        for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
            metac_entry_t * p_entry = metac_entry_from_entry_db(
                tcs[tc_inx].p_entry_db,
                tcs[tc_inx].p_decl_loc,
                tcs[tc_inx].varname);
            fail_unless((p_entry == NULL) == (tcs[tc_inx].expected_cdecl == NULL), "tc_inx %d: unexpected result %p",
                tc_inx, p_entry);
            if (p_entry != NULL) {
                char * cdecl = metac_entry_cdecl(p_entry);
                fail_unless(strcmp(tcs[tc_inx].expected_cdecl, cdecl)==0, "tc_inx %d: unexpected result %s, got %s",
                    tc_inx, tcs[tc_inx].expected_cdecl, cdecl);
                if (cdecl != NULL) {
                    free(cdecl);
                }
            }
        }
    }
}END_TEST

METAC_START_TEST(test_same_name_1_line) {
    metac_entry_t *p1, *p2;
    char * cdecl, * expected_cdecl;
    WITH_METAC_DECLLOC(local_data_loc, 
        int same_name = 1;
        p1 = METAC_ENTRY_FROM_DECLLOC(local_data_loc, same_name);
        fail_unless(p1 != NULL, "p1 is NULL");
        cdecl = metac_entry_cdecl(p1);
        fail_unless(cdecl != NULL, "cdecl1 is NULL");
        expected_cdecl = "int same_name";
        fail_unless(strcmp(cdecl, expected_cdecl) == 0, "got %s, expected %s", cdecl, expected_cdecl);
        free(cdecl);
        fail_unless(p1 != NULL);
        {
            WITH_METAC_DECLLOC(local_data_loc,
                long same_name=2; 
                p2 = METAC_ENTRY_FROM_DECLLOC(local_data_loc, same_name);
                fail_unless(p2 != NULL, "p2 is NULL");
                cdecl = metac_entry_cdecl(p2);
                fail_unless(cdecl != NULL, "cdecl2 is NULL");
                expected_cdecl = "long int same_name";
                fail_unless(strcmp(cdecl, expected_cdecl) == 0, "got %s, expected %s", cdecl, expected_cdecl);
                free(cdecl);
            );
        }
    );
}END_TEST
