/*
 * metac_value_scheme_ut_001.c
 *
 *  Created on: Feb 21, 2020
 *      Author: mralex
 */

#include <stdarg.h>

#include "check_ext.h"
#include "metac_debug.h"	/* msg_stderr, ...*/
#include "metac/metac_type.h"
#include "metac/metac_value_scheme.h"


typedef struct _basic_tree {
	int * data;
	struct _basic_tree *desc[2];
}basic_tree_t;
METAC_TYPE_GENERATE_AND_IMPORT(basic_tree_t);

START_TEST(metac_value_scheme_001) {
	struct metac_value_scheme * p_metac_value_scheme;
	struct metac_value_scheme * p_metac_value_scheme_copy;

	p_metac_value_scheme = metac_value_scheme_create(
			&METAC_TYPE_NAME(basic_tree_t));

	fail_unless(p_metac_value_scheme != NULL, "Wasn't able to generate value_scheme for basic_tree_t");
	fail_unless(metac_value_scheme_is_indexable(p_metac_value_scheme) == 1);
	fail_unless(metac_value_scheme_is_hierarchy_top(p_metac_value_scheme) == 1);
	fail_unless(metac_value_scheme_is_hierarchy_member(p_metac_value_scheme) == 0);
	fail_unless(metac_value_scheme_get_parent_value_scheme(p_metac_value_scheme) == NULL);
	fail_unless(metac_value_scheme_is_hierachy(p_metac_value_scheme) == 1);
	fail_unless(metac_value_scheme_is_pointer(p_metac_value_scheme) == 0);
	fail_unless(metac_value_scheme_is_array(p_metac_value_scheme) == 0);

	p_metac_value_scheme_copy = metac_value_scheme_get(p_metac_value_scheme);
	fail_unless(p_metac_value_scheme_copy != NULL);

	metac_value_scheme_put(&p_metac_value_scheme);
	metac_value_scheme_put(&p_metac_value_scheme_copy);

}END_TEST

int main(void){
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(metac_value_scheme_ut){
					ADD_TEST(metac_value_scheme_001);
				}END_CASE
			);
		}END_SUITE
	);
}