/*
 * metac_internals_ut_001.c
 *
 *  Created on: Oct 6, 2019
 *      Author: mralex
 */

#include "metac_type.h"
#include "metac_internals.h"

#include "check_ext.h"

struct precompiled_type_builder;
int precompiled_type_builder_init(
		struct precompiled_type_builder *			p_precompiled_type_builder);

//METAC_TYPE_GENERATE(anon_struct_with_anon_elements);
START_TEST(internals_ut) {
	//struct metac_type * p_type = &METAC_TYPE_NAME(anon_struct_with_anon_elements);
	struct precompiled_type_builder * p_precompiled_type_builder = NULL;
//	precompiled_type_builder_init(p_precompiled_type_builder);
//	precompiled_type_builder_add_element_type_from_ptr(p_precompiled_type_builder, "ptr", p_type);
	// should create element_type and call callback with data

}END_TEST

int main(void){
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(type_smoke){
					ADD_TEST(internals_ut);
				}END_CASE
			);
		}END_SUITE
	);
}

