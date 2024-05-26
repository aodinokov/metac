#include "metac/test.h"

#include "demodb.c"

METAC_START_TEST(append_test) {
    struct {
        person_t * p_in;
        int expected_err;
    }tcs[] = {
        {
            .p_in = NULL,
            .expected_err = 1,
        },
        {
            .p_in = (person_t[]){{
                .firstname="Joe",
                .lastname="Doe",
                .age = 43,
                .marital_status = msMarried,
            }},
            .expected_err = 0,
        },
        {
            .p_in = (person_t[]){{
                .firstname="Jane",
                .lastname="Doe",
                .age = 34,
                .marital_status = msMarried,
            }},
            .expected_err = 0,
        },
        {
            .p_in = (person_t[]){{
                .firstname="Jack",
                .lastname="Doe",
                .age = 3,
                .marital_status = msSingle,
            }},
            .expected_err = 0,
        },
    };
    db_t * p_db =  new_db();
    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        int res = db_append(&p_db, tcs[tc_inx].p_in);
        fail_unless((res != 0) == (tcs[tc_inx].expected_err != 0), "unexpected err result %i, expected %i", 
            res, tcs[tc_inx].expected_err);
    }
    db_delete(p_db);
}END_TEST