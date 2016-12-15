/*
 * metac_ut_001.c
 *
 *  Created on: Oct 3, 2015
 *      Author: mralex
 */


#include "check_ext.h"
#include "metac_object.h"

/*
 * ideas for UT:
 * 1. objects of exported metatype
 * 1.1. define type (many different cases:
 *  base types (https://en.wikipedia.org/wiki/C_data_types), pointers,
 *  typedefs, enums, arrays, structures, unions, their combinations)
 *  bonus:va_list, arrays with undefined length?
 * 1.2. export metatype based on that type
 * 1.3. create object by using the metatype definition
 * 1.4. check that fields offsets and lengths are the same as for initial type
 *
 * 2. array of objects of exported metatype
 * 2.1. define type (see 1.1)
 * 2.2. export metatype based on that type
 * 2.3. create array of objeccts by using the metatype definition
 * 2.4. check that fields offsets and lengths are the same as for array of initial type
 *
 * 3. check that metatype is correctly builded (optional - covered by 1 and 2)
 * 4. check that function metatype is correctly builded (result type, params)6
 * 5. check basic functions to navigate by metatype
 * 6. check basic functions to navigate by object, created by metatype
 *
 * 7. serialization/deserialization - TBD: status from json - not in this ut
 */


/**
 * Know issues and workarounds:
 * 1. signed/unsinged <char|short|int|long>, long long, void*(all pointers and arrays) won't work
 * because they are not one-work types.
 * use typedefs to export that types via METAC
 */

/* export base types */
METAC_TYPE_IMPORT(char);
METAC_TYPE_IMPORT(short);
METAC_TYPE_IMPORT(int);
METAC_TYPE_IMPORT(long);
METAC_TYPE_IMPORT(float);
METAC_TYPE_IMPORT(double);

/* create typedefs for non-one-word base types and export them */
/*char*/
typedef char char_t;
typedef signed char schar_t;
typedef unsigned char uchar_t;

METAC_TYPE_IMPORT(char_t);
METAC_TYPE_IMPORT(schar_t);
METAC_TYPE_IMPORT(uchar_t);

/*short*/
typedef short short_t;
typedef short int shortint_t;
typedef signed short sshort_t;
typedef signed short int sshortint_t;
typedef unsigned short ushort_t;
typedef unsigned short int ushortint_t;

METAC_TYPE_IMPORT(short_t);
METAC_TYPE_IMPORT(shortint_t);
METAC_TYPE_IMPORT(sshort_t);
METAC_TYPE_IMPORT(sshortint_t);
METAC_TYPE_IMPORT(ushort_t);
METAC_TYPE_IMPORT(ushortint_t);

/*int*/
typedef int int_t;
typedef signed signed_t;
typedef signed int sint_t;
typedef unsigned unsigned_t;
typedef unsigned int uint_t;

METAC_TYPE_IMPORT(int_t);
METAC_TYPE_IMPORT(signed_t);
METAC_TYPE_IMPORT(sint_t);
METAC_TYPE_IMPORT(unsigned_t);
METAC_TYPE_IMPORT(uint_t);

/*long*/
typedef long long_t;
typedef long int longint_t;
typedef signed long slong_t;
typedef signed long int slongint_t;
typedef unsigned long ulong_t;
typedef unsigned long int ulongint_t;

METAC_TYPE_IMPORT(long_t);
METAC_TYPE_IMPORT(longint_t);
METAC_TYPE_IMPORT(slong_t);
METAC_TYPE_IMPORT(slongint_t);
METAC_TYPE_IMPORT(ulong_t);
METAC_TYPE_IMPORT(ulongint_t);

/*long long*/
typedef long long llong_t;
typedef long long int llongint_t;
typedef signed long long sllong_t;
typedef signed long long int sllongint_t;
typedef unsigned long long ullong_t;
typedef unsigned long long int ullongint_t;

METAC_TYPE_IMPORT(llong_t);
METAC_TYPE_IMPORT(llongint_t);
METAC_TYPE_IMPORT(sllong_t);
METAC_TYPE_IMPORT(sllongint_t);
METAC_TYPE_IMPORT(ullong_t);
METAC_TYPE_IMPORT(ullongint_t);

/*float*/
typedef float float_t;
METAC_TYPE_IMPORT(float_t);

/*double*/
typedef double double_t;
METAC_TYPE_IMPORT(double_t);

/*long double*/
typedef long double ldouble_t;
METAC_TYPE_IMPORT(ldouble_t);

/* pointers */
typedef void* voidptr_t;
typedef void** voidptrptr_t;
typedef char* charptr_t;
/* skip other types */

METAC_TYPE_IMPORT(voidptr_t);
METAC_TYPE_IMPORT(voidptrptr_t);
METAC_TYPE_IMPORT(charptr_t);

/* enums */
typedef enum _enum_{
	_eZero = 0,
	_eOne,
	_eTen = 10,
	_eEleven,
	_eTwelve,
}enum_t;

typedef enum{
	aeMinus = -1,
	aeZero = 0,
	aeOne = 1,
	aeTen = 10,
	aeEleven,
	aeTwelve,
}anon_enum_t;
METAC_TYPE_IMPORT(enum_t);
METAC_TYPE_IMPORT(anon_enum_t);

/* arrays */
typedef char_t char_array5_t[5];
METAC_TYPE_IMPORT(char_array5_t);

/* unions */
typedef union _union_{
   int d;
   char f;
}union_t;
METAC_TYPE_IMPORT(union_t);

/* struct */
typedef struct _struct_
{
  unsigned int widthValidated;
  unsigned int heightValidated;
}struct_t;
METAC_TYPE_IMPORT(struct_t);

/* bit fields */
typedef struct _bit_fields_
{
  unsigned int widthValidated : 1;
  unsigned int heightValidated : 2;
}bit_fields_t;
METAC_TYPE_IMPORT(bit_fields_t);

/*TODO: some combinations*/

#define GENERAL_TYPE_SMOKE(_type_) \
do{ \
	unsigned int data_len; \
	_type_ *ptr; \
	struct metac_object *object; \
	struct metac_type *type = METAC_TYPE(_type_); \
	\
	fail_unless(metac_type_byte_size(type) == sizeof(_type_), "metac_type_byte_size returned incorrect value"); \
	\
	object = metac_object_create(METAC_TYPE(_type_)); \
	fail_unless(object != NULL, "object wasn't created"); \
	\
	fail_unless(metac_object_type(object) == METAC_TYPE(_type_), \
			"metac_object_type returned incorrect pointer on type"); \
	\
	ptr = (_type_ *)metac_object_ptr(object, &data_len); \
	fail_unless(ptr != NULL, "metac_object_ptr must return address"); \
	fail_unless(data_len == sizeof(_type_), "metac_object_ptr object has incorrect size %d instead of %d", \
			(int)data_len, (int)sizeof(_type_)); \
	\
	metac_object_destroy(object); \
	\
	mark_point(); \
}while(0)


START_TEST(basic_types_smoke) {
	GENERAL_TYPE_SMOKE(char);
	GENERAL_TYPE_SMOKE(short);
	GENERAL_TYPE_SMOKE(int);
	GENERAL_TYPE_SMOKE(long);
	GENERAL_TYPE_SMOKE(float);
	GENERAL_TYPE_SMOKE(double);

	GENERAL_TYPE_SMOKE(char_t);
	GENERAL_TYPE_SMOKE(schar_t);
	GENERAL_TYPE_SMOKE(uchar_t);

	GENERAL_TYPE_SMOKE(short_t);
	GENERAL_TYPE_SMOKE(shortint_t);
	GENERAL_TYPE_SMOKE(sshort_t);
	GENERAL_TYPE_SMOKE(sshortint_t);
	GENERAL_TYPE_SMOKE(ushort_t);
	GENERAL_TYPE_SMOKE(ushortint_t);

	GENERAL_TYPE_SMOKE(int_t);
	GENERAL_TYPE_SMOKE(signed_t);
	GENERAL_TYPE_SMOKE(sint_t);
	GENERAL_TYPE_SMOKE(unsigned_t);
	GENERAL_TYPE_SMOKE(uint_t);

	GENERAL_TYPE_SMOKE(long_t);
	GENERAL_TYPE_SMOKE(longint_t);
	GENERAL_TYPE_SMOKE(slong_t);
	GENERAL_TYPE_SMOKE(slongint_t);
	GENERAL_TYPE_SMOKE(ulong_t);
	GENERAL_TYPE_SMOKE(ulongint_t);

	GENERAL_TYPE_SMOKE(llong_t);
	GENERAL_TYPE_SMOKE(llongint_t);
	GENERAL_TYPE_SMOKE(sllong_t);
	GENERAL_TYPE_SMOKE(sllongint_t);
	GENERAL_TYPE_SMOKE(ullong_t);
	GENERAL_TYPE_SMOKE(ullongint_t);

	GENERAL_TYPE_SMOKE(float_t);
	GENERAL_TYPE_SMOKE(double_t);
	GENERAL_TYPE_SMOKE(ldouble_t);

	GENERAL_TYPE_SMOKE(voidptr_t);
	GENERAL_TYPE_SMOKE(voidptrptr_t);
	GENERAL_TYPE_SMOKE(charptr_t);

	GENERAL_TYPE_SMOKE(enum_t);
	GENERAL_TYPE_SMOKE(anon_enum_t);

	GENERAL_TYPE_SMOKE(char_array5_t);

	GENERAL_TYPE_SMOKE(union_t);
	GENERAL_TYPE_SMOKE(struct_t);
	GENERAL_TYPE_SMOKE(bit_fields_t);

}END_TEST



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
METAC_TYPE_IMPORT(check_all_types1_t);

START_TEST(check_all_types1) {
	struct metac_type *type = METAC_TYPE(check_all_types1_t);
	fail_unless(metac_type_child_num(metac_type_typedef_skip(type)) == 19, "Incorrect member count");
}END_TEST


START_TEST(check_object_create1) {
	struct metac_object *object;
	struct metac_type *member_type;
	struct metac_object *member_object;
	check_all_types1_t *s;
	unsigned int data_len = 0;

	object = metac_object_create(METAC_TYPE(check_all_types1_t));
	fail_unless(object != NULL, "object wasn't created");

	fail_unless(metac_object_type(object) == METAC_TYPE(check_all_types1_t),
			"metac_object_type returned incorrect pointer on type");

	s = (check_all_types1_t *)metac_object_ptr(object, &data_len);
	fail_unless(s != NULL, "metac_object_ptr must return address");
	fail_unless(data_len == sizeof(check_all_types1_t), "metac_object_ptr object has incorrect size %d instead of %d",
			(int)data_len, (int)sizeof(check_all_types1_t));

	/*check first member*/
	member_object = metac_object_structure_member_by_name(object, "xschar");
	fail_unless(member_object != NULL, "xschar member must present");

	/*check offset*/
	fail_unless(&s->xschar == metac_object_ptr(member_object, &data_len), "incorrect xschar offset detected %p instead of %p",
			metac_object_ptr(member_object, NULL),
			&s->xschar);

	fail_unless(metac_object_structure_member_by_name(object, "xaschar1") == NULL, "xaschar1 member must NOT present");

	/*check with array TODO: probably we could create macro - because it repeats the test with xschar*/
	member_object = metac_object_structure_member_by_name(object, "xaschar");
	fail_unless(member_object != NULL, "xaschar member must present");

	/*check offset*/
	fail_unless(&s->xaschar == metac_object_ptr(member_object, &data_len), "incorrect xaschar offset detected %p instead of %p",
			metac_object_ptr(member_object, NULL),
			&s->xaschar);
	fail_unless(data_len == sizeof(s->xaschar), "metac_object_ptr object has incorrect size %d instead of %d",
			(int)data_len, (int)sizeof(s->xaschar));


	/*check third member*/
	member_object = metac_object_structure_member_by_name(object, "xuchar");
	fail_unless(member_object != NULL, "xuchar member must present");

	/*check offset*/
	fail_unless(&s->xuchar == metac_object_ptr(member_object, &data_len), "incorrect xuchar offset detected %p instead of %p",
			metac_object_ptr(member_object, NULL),
			&s->xuchar);
	fail_unless(data_len == sizeof(s->xuchar), "metac_object_ptr object has incorrect size %d instead of %d",
			(int)data_len, (int)sizeof(s->xuchar));


	metac_object_destroy(object);
}END_TEST

int main(void){
	return run_suite(
		START_SUITE(suite_1){
			ADD_CASE(
				START_CASE(basic_case){
					ADD_TEST(basic_types_smoke);
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

