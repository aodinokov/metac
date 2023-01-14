/*
 * metac_scheme_ut_001.c
 *
 *  Created on: Feb 21, 2020
 *      Author: mralex
 */

#include <metac/metac_scheme.h>
#include <stdarg.h>

#include "check_ext.h"
#include "metac_debug.h"	/* msg_stderr, ...*/
#include "metac/metac_type.h"

typedef struct _basic_tree {
    int *data;
    struct _basic_tree *desc[2];
} basic_tree_t;
METAC_TYPE_GENERATE_AND_IMPORT(basic_tree_t);

START_TEST( metac_scheme_001) {
    int i;
    struct metac_scheme *p_metac_scheme;
    struct metac_scheme *p_metac_scheme_copy;

    p_metac_scheme = metac_scheme_create(&METAC_TYPE_NAME(basic_tree_t), NULL,
            1);

    fail_unless(p_metac_scheme != NULL,
            "Wasn't able to generate value_scheme for basic_tree_t");
    p_metac_scheme_copy = metac_scheme_get(p_metac_scheme);
    fail_unless(p_metac_scheme_copy != NULL);
    metac_scheme_put(&p_metac_scheme_copy);

    fail_unless(metac_scheme_is_indexable(p_metac_scheme) == 1);
    fail_unless(metac_scheme_is_hierarchy_top_scheme(p_metac_scheme) == 1);
    fail_unless(
            metac_hierarchy_top_scheme_get_members_count(p_metac_scheme) == 2,
            "got %d instead 2\n",
            metac_hierarchy_top_scheme_get_members_count(p_metac_scheme));

    for (i = 0;
            i < metac_hierarchy_top_scheme_get_members_count(p_metac_scheme);
            ++i) {

        struct metac_scheme *p_metac_hierarchy_member_scheme;

        p_metac_hierarchy_member_scheme =
                metac_hierarchy_top_scheme_get_hierarchy_member_scheme(
                        p_metac_scheme, i);
        fail_unless(p_metac_hierarchy_member_scheme != NULL);

        fail_unless(
                metac_scheme_is_hierarchy_member_scheme(
                        p_metac_hierarchy_member_scheme) == 1);
        fail_unless(
                metac_hierarchy_member_scheme_get_parent_scheme(
                        p_metac_hierarchy_member_scheme) == p_metac_scheme);

        switch (i) {
        case 0:
            fail_unless(
                    metac_scheme_is_pointer_scheme(
                            p_metac_hierarchy_member_scheme) == 1);
            break;
        case 1:
            fail_unless(
                    metac_scheme_is_array_scheme(
                            p_metac_hierarchy_member_scheme) == 1);
            break;
        }

        metac_scheme_put(&p_metac_hierarchy_member_scheme);
    }

    fail_unless(metac_scheme_is_hierarchy_member_scheme(p_metac_scheme) == 0);
    fail_unless(
            metac_hierarchy_member_scheme_get_parent_scheme(p_metac_scheme)
                    == NULL);
    fail_unless(metac_scheme_is_hierachy_scheme(p_metac_scheme) == 1);
    fail_unless(metac_scheme_is_pointer_scheme(p_metac_scheme) == 0);
    fail_unless(metac_scheme_is_array_scheme(p_metac_scheme) == 0);

    metac_scheme_put(&p_metac_scheme);

}
END_TEST

int main(void) {
    return run_suite(
    START_SUITE(type_suite) {
        ADD_CASE(
                START_CASE(metac_scheme_ut) {
                    ADD_TEST(metac_scheme_001);
                }END_CASE
        );
    }END_SUITE);
}
