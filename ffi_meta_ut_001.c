/*
 * ffi_meta_ut_001.c
 *
 *  Created on: Oct 3, 2015
 *      Author: mralex
 */


#include "check_ext.h"
#include "ffi_meta.h"

struct char_struct_s {
	signed char xschar;
	signed char xaschar[10];
	unsigned char xuchar;
	unsigned char xauchar[20];
};

typedef struct check_all_types1_s {
	signed char xschar;
	signed char xaschar[10];
	unsigned char xuchar;
	unsigned char xauchar[20];
	signed short xsshort;
	signed short xasshort[30];
	unsigned short xushort;
	unsigned short xaushort[40];
	signed int xsint;
	signed int xasint[10];
	unsigned int xuint;
	unsigned int xauint[10];
	signed long xslong;
	signed long xaslong[10];
	unsigned long xulong;
	unsigned long xaulong[10];
	struct check_all_types1_s *p_struct;
	struct check_all_types1_s *p_astruct[10];
	struct char_struct_s astruct[10];
}check_all_types1_t;
FFI_META_EXPORT_TYPE(check_all_types1_t);

START_TEST(check_all_types1) {
	struct ffi_meta_type *type = FFI_META_TYPE(check_all_types1_t);
	fail_unless(ffi_meta_structure_member_count(type) == 19, "Incorrect member count");
}END_TEST


START_TEST(check_object_create1) {
	struct ffi_meta_object *object;
	struct ffi_meta_type *member_type;
	struct ffi_meta_object *member_object;
	check_all_types1_t *s;

	object = ffi_meta_object_create(FFI_META_TYPE(check_all_types1_t));
	fail_unless(object != NULL, "object wasn't created");

	fail_unless(ffi_meta_object_type(object) == FFI_META_TYPE(check_all_types1_t),
			"ffi_meta_object_type returned incorrect pointer on type");

	s = (check_all_types1_t *)ffi_meta_object_ptr(object);
	fail_unless(s != NULL, "ffi_meta_object_ptr must return address");

	/*check first member*/
	member_object = ffi_meta_object_structure_member_by_name(object, "xschar");
	fail_unless(member_object != NULL, "xschar member must present");

	/*check offset*/
	fail_unless(&s->xschar == ffi_meta_object_ptr(member_object), "incorrect xschar offset detected %p instead of %p",
			ffi_meta_object_ptr(member_object),
			&s->xschar);

	fail_unless(ffi_meta_object_structure_member_by_name(object, "xaschar1") == NULL, "xaschar1 member must NOT present");

	/*check with array TODO: probably we could create macro - because it repeats the test with xschar*/
	member_object = ffi_meta_object_structure_member_by_name(object, "xaschar");
	fail_unless(member_object != NULL, "xaschar member must present");

	/*check offset*/
	fail_unless(&s->xaschar == ffi_meta_object_ptr(member_object), "incorrect xaschar offset detected %p instead of %p",
			ffi_meta_object_ptr(member_object),
			&s->xaschar);

	/*check third member*/
	member_object = ffi_meta_object_structure_member_by_name(object, "xuchar");
	fail_unless(member_object != NULL, "xuchar member must present");

	/*check offset*/
	fail_unless(&s->xuchar == ffi_meta_object_ptr(member_object), "incorrect xuchar offset detected %p instead of %p",
			ffi_meta_object_ptr(member_object),
			&s->xuchar);


	ffi_meta_object_destroy(object);
}END_TEST


int main(void){
	return run_suite(
		START_SUITE(suite_1){
			ADD_CASE(
				START_CASE(basic_case){
					ADD_TEST(check_all_types1);
				}END_CASE
			);
			ADD_CASE(
				START_CASE(dynamic_case){
					ADD_TEST(check_object_create1);
				}END_CASE
			);
		}END_SUITE
	);
}

