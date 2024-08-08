#include "metac/test.h"
#include "metac/reflect.h"

#include "demodb.c"

// define a tag_map for demodb.c
METAC_TAG_MAP_NEW(new_demodb_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member)},)

    METAC_TAG_MAP_ENTRY_FROM_TYPE(db_t)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n="data"}),
            METAC_COUNT_BY(count)
        )
    METAC_TAG_MAP_ENTRY_END

    METAC_TAG_MAP_ENTRY_FROM_TYPE(person_t)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n="firstname"}),
            METAC_ZERO_ENDED_STRING()
        )
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n="lastname"}),
            METAC_ZERO_ENDED_STRING()
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END

METAC_START_TEST(append_test) {
    struct {
        person_t * p_in;
        int expected_err;
        char * expected_string;
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
            .expected_string = "(db_t []){{.count = 1, .data = {{.firstname = \"Joe\", .lastname = \"Doe\", .age = 43, .marital_status = msMarried,},},},}",
        },
        {
            .p_in = (person_t[]){{
                .firstname="Jane",
                .lastname="Doe",
                .age = 34,
                .marital_status = msMarried,
            }},
            .expected_err = 0,
            .expected_string = "(db_t []){{.count = 2, .data = {"
                "{.firstname = \"Joe\", .lastname = \"Doe\", .age = 43, .marital_status = msMarried,}, "
                "{.firstname = \"Jane\", .lastname = \"Doe\", .age = 34, .marital_status = msMarried,},"
            "},},}",
        },
        {
            .p_in = (person_t[]){{
                .firstname="Jack",
                .lastname="Doe",
                .age = 3,
                .marital_status = msSingle,
            }},
            .expected_err = 0,
            .expected_string = "(db_t []){{.count = 3, .data = {"
                "{.firstname = \"Joe\", .lastname = \"Doe\", .age = 43, .marital_status = msMarried,}, "
                "{.firstname = \"Jane\", .lastname = \"Doe\", .age = 34, .marital_status = msMarried,}, "
                "{.firstname = \"Jack\", .lastname = \"Doe\", .age = 3, .marital_status = msSingle,},"
            "},},}",
        },
    };
    metac_tag_map_t * p_tag_map = new_demodb_tag_map();
    WITH_METAC_DECLLOC(loc, db_t * p_db = new_db());
    fail_unless(p_tag_map != NULL, "new_demodb_tag_map failed");
    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        int res = db_append(&p_db, tcs[tc_inx].p_in);
        fail_unless((res != 0) == (tcs[tc_inx].expected_err != 0), "unexpected err result %i, expected %i", 
            res, tcs[tc_inx].expected_err);
        
        if (tcs[tc_inx].expected_err == 0 && res == 0) {
            // let's print the data
            char * res_str;
            metac_value_t *p_db_value = METAC_VALUE_FROM_DECLLOC(loc, p_db);
            fail_unless(p_db_value != NULL);

            res_str = metac_value_string_ex(p_db_value, METAC_WMODE_deep, p_tag_map);
            fail_unless(res_str != NULL, "tc_inx %i: string is NULL", tc_inx);

            fail_unless(tcs[tc_inx].expected_string != NULL &&
                strcmp(res_str, tcs[tc_inx].expected_string) == 0, "tc_inx %i: got \"%s\", expected \"%s\"",
                tc_inx, res_str, tcs[tc_inx].expected_string);

            free(res_str);
            metac_value_delete(p_db_value);
        }
    }
    db_delete(p_db);
    metac_tag_map_delete(p_tag_map);
}END_TEST

METAC_START_TEST(deep_test) {
    metac_tag_map_t * p_tag_map = new_demodb_tag_map();
    WITH_METAC_DECLLOC(loc, 
        db_t * p_db = new_db(), *p_db_backup = NULL);
    fail_unless(p_tag_map != NULL, "new_demodb_tag_map failed");

    fail_unless(db_append(&p_db, (person_t[]){{
		.firstname="Joe",
		.lastname="Doe",
		.age = 43,
		.marital_status = msMarried,
	}}) == 0, "db_append 1 failed");

    /* create values */
    metac_value_t *p_db_value = METAC_VALUE_FROM_DECLLOC(loc, p_db);
    fail_unless(p_db_value != NULL);
    metac_value_t *p_db_backup_value = METAC_VALUE_FROM_DECLLOC(loc, p_db_backup);
    fail_unless(p_db_backup_value != NULL);

    metac_value_memory_map_mode_t mode = {.memory_block_mode = METAC_MMODE_dag,};

    fail_unless(metac_value_copy_ex(p_db_value, p_db_backup_value,
        &mode, calloc, free, p_tag_map) != NULL, "copy failed");

    char * str = NULL, * expected_str = NULL;

    str = metac_value_string_ex(p_db_backup_value, METAC_WMODE_deep, p_tag_map);
    expected_str = "(db_t []){{.count = 1, .data = {{.firstname = \"Joe\", .lastname = \"Doe\", .age = 43, .marital_status = msMarried,},},},}";

    fail_unless(str != NULL && strcmp(str, expected_str) == 0, "got %s, expected %s", str, expected_str);
    free(str);

    fail_unless(metac_value_equal_ex(p_db_value, p_db_backup_value,
        &mode, p_tag_map) == 1, "expected db and backup be equal");

    // we even can use db api for copy
    fail_unless(db_append(&p_db_backup, (person_t[]){{
		.firstname="Jane",
		.lastname="Doe",
		.age = 34,
		.marital_status = msMarried,
	}}) == 0, "db_append 1 failed");

    str = metac_value_string_ex(p_db_backup_value, METAC_WMODE_deep, p_tag_map);
    expected_str = "(db_t []){{.count = 2, .data = {"
        "{.firstname = \"Joe\", .lastname = \"Doe\", .age = 43, .marital_status = msMarried,}, "
        "{.firstname = \"Jane\", .lastname = \"Doe\", .age = 34, .marital_status = msMarried,},"
    "},},}";

    fail_unless(str != NULL && strcmp(str, expected_str) == 0, "got %s, expected %s", str, expected_str);
    free(str);

    fail_unless(metac_value_equal_ex(p_db_value, p_db_backup_value,
        &mode, p_tag_map) == 0, "expected db and backup be NOT equal");

    // and clean data with 'deep function' instead of db api
    metac_value_memory_map_non_handled_mode_t fmode = {0, };
    fail_unless(metac_value_free_ex(p_db_value, &fmode, free, p_tag_map) != 0, "wasn't able to free db");

    metac_value_delete(p_db_backup_value);
    metac_value_delete(p_db_value);

    db_delete(p_db_backup);
    metac_tag_map_delete(p_tag_map);
}END_TEST