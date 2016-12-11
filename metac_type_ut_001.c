/*
 * metac_ut_001.c
 *
 *  Created on: Oct 3, 2015
 *      Author: mralex
 */


#include "check_ext.h"
#include "metac_type.h"

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
METAC_EXPORT_TYPE(char);
METAC_EXPORT_TYPE(short);
METAC_EXPORT_TYPE(int);
METAC_EXPORT_TYPE(long);
METAC_EXPORT_TYPE(float);
METAC_EXPORT_TYPE(double);

/* create typedefs for non-one-word base types and export them */
/*char*/
typedef char char_t;
typedef signed char schar_t;
typedef unsigned char uchar_t;

METAC_EXPORT_TYPE(char_t);
METAC_EXPORT_TYPE(schar_t);
METAC_EXPORT_TYPE(uchar_t);

/*short*/
typedef short short_t;
typedef short int shortint_t;
typedef signed short sshort_t;
typedef signed short int sshortint_t;
typedef unsigned short ushort_t;
typedef unsigned short int ushortint_t;

METAC_EXPORT_TYPE(short_t);
METAC_EXPORT_TYPE(shortint_t);
METAC_EXPORT_TYPE(sshort_t);
METAC_EXPORT_TYPE(sshortint_t);
METAC_EXPORT_TYPE(ushort_t);
METAC_EXPORT_TYPE(ushortint_t);

/*int*/
typedef int int_t;
typedef signed signed_t;
typedef signed int sint_t;
typedef unsigned unsigned_t;
typedef unsigned int uint_t;

METAC_EXPORT_TYPE(int_t);
METAC_EXPORT_TYPE(signed_t);
METAC_EXPORT_TYPE(sint_t);
METAC_EXPORT_TYPE(unsigned_t);
METAC_EXPORT_TYPE(uint_t);

/*long*/
typedef long long_t;
typedef long int longint_t;
typedef signed long slong_t;
typedef signed long int slongint_t;
typedef unsigned long ulong_t;
typedef unsigned long int ulongint_t;

METAC_EXPORT_TYPE(long_t);
METAC_EXPORT_TYPE(longint_t);
METAC_EXPORT_TYPE(slong_t);
METAC_EXPORT_TYPE(slongint_t);
METAC_EXPORT_TYPE(ulong_t);
METAC_EXPORT_TYPE(ulongint_t);

/*long long*/
typedef long long llong_t;
typedef long long int llongint_t;
typedef signed long long sllong_t;
typedef signed long long int sllongint_t;
typedef unsigned long long ullong_t;
typedef unsigned long long int ullongint_t;

METAC_EXPORT_TYPE(llong_t);
METAC_EXPORT_TYPE(llongint_t);
METAC_EXPORT_TYPE(sllong_t);
METAC_EXPORT_TYPE(sllongint_t);
METAC_EXPORT_TYPE(ullong_t);
METAC_EXPORT_TYPE(ullongint_t);

/*float*/
typedef float float_t;
METAC_EXPORT_TYPE(float_t);

/*double*/
typedef double double_t;
METAC_EXPORT_TYPE(double_t);

/*long double*/
typedef long double ldouble_t;
METAC_EXPORT_TYPE(ldouble_t);

/* pointers */
typedef void* voidptr_t;
typedef void** voidptrptr_t;
typedef char* charptr_t;
/* skip other types */

METAC_EXPORT_TYPE(voidptr_t);
METAC_EXPORT_TYPE(voidptrptr_t);
METAC_EXPORT_TYPE(charptr_t);

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
METAC_EXPORT_TYPE(enum_t);
METAC_EXPORT_TYPE(anon_enum_t);

/* arrays */
typedef char_t char_array5_t[5];
METAC_EXPORT_TYPE(char_array5_t);

/* unions */
typedef union _union_{
   int d;
   char f;
}union_t;
METAC_EXPORT_TYPE(union_t);

/* struct */
typedef struct _struct_
{
  unsigned int widthValidated;
  unsigned int heightValidated;
}struct_t;
METAC_EXPORT_TYPE(struct_t);

/* bit fields */
typedef struct _bit_fields_
{
  unsigned int widthValidated : 9;
  unsigned int heightValidated : 12;
}bit_fields_t;
METAC_EXPORT_TYPE(bit_fields_t);

/*TODO: some combinations??? */

/* function ptr */
typedef int_t (*func_ptr_t)(bit_fields_t *arg);
METAC_EXPORT_TYPE(func_ptr_t);

/* function */
int_t func_t(bit_fields_t *arg) {if (arg)return 1; return 0;}
METAC_EXPORT_TYPE(func_t);


#define GENERAL_TYPE_SMOKE(_type_, _s_type_) \
do{ \
	_type_ *ptr = NULL; \
	struct metac_type *type = METAC_TYPE(_type_); \
	struct metac_type *typedef_skip_type = metac_type_typedef_skip(type); \
	char * type_name = metac_type_name(type); \
	char * type_name_copy = strdup((type_name!=NULL)?type_name:""), \
		 * tmp = strchr(type_name_copy, ' '); \
	if (tmp != NULL)*tmp = '\0'; /*workaround for long int and short int - get only first word*/ \
	\
	fail_unless(metac_type_byte_size(type) == sizeof(_type_), "metac_type_byte_size returned incorrect value for " #_type_); \
	fail_unless(metac_type(typedef_skip_type) == _s_type_, "must be " #_s_type_ ", but it's 0x%x", (int)metac_type(typedef_skip_type)); \
	fail_unless(strcmp(type_name_copy, #_type_ ) == 0, "type name returned '%s' instead of '" #_type_ "'", metac_type_name(type));\
	\
	free(type_name_copy);\
	mark_point(); \
} while(0)


START_TEST(general_type_smoke) {
	GENERAL_TYPE_SMOKE(char, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(short, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(int, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(long, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(float, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(double, DW_TAG_base_type);

	GENERAL_TYPE_SMOKE(char_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(schar_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(uchar_t, DW_TAG_base_type);

	GENERAL_TYPE_SMOKE(short_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(shortint_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(sshort_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(sshortint_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(ushort_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(ushortint_t, DW_TAG_base_type);

	GENERAL_TYPE_SMOKE(int_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(signed_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(sint_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(unsigned_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(uint_t, DW_TAG_base_type);

	GENERAL_TYPE_SMOKE(long_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(longint_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(slong_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(slongint_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(ulong_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(ulongint_t, DW_TAG_base_type);

	GENERAL_TYPE_SMOKE(llong_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(llongint_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(sllong_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(sllongint_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(ullong_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(ullongint_t, DW_TAG_base_type);

	GENERAL_TYPE_SMOKE(float_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(double_t, DW_TAG_base_type);
	GENERAL_TYPE_SMOKE(ldouble_t, DW_TAG_base_type);

	GENERAL_TYPE_SMOKE(voidptr_t, DW_TAG_pointer_type);
	GENERAL_TYPE_SMOKE(voidptrptr_t, DW_TAG_pointer_type);
	GENERAL_TYPE_SMOKE(charptr_t, DW_TAG_pointer_type);

	GENERAL_TYPE_SMOKE(enum_t, DW_TAG_enumeration_type);
	GENERAL_TYPE_SMOKE(anon_enum_t, DW_TAG_enumeration_type);

	GENERAL_TYPE_SMOKE(char_array5_t, DW_TAG_array_type);

	GENERAL_TYPE_SMOKE(union_t, DW_TAG_union_type);

	GENERAL_TYPE_SMOKE(struct_t, DW_TAG_structure_type);
	GENERAL_TYPE_SMOKE(bit_fields_t, DW_TAG_structure_type);

	GENERAL_TYPE_SMOKE(func_ptr_t, DW_TAG_pointer_type);
}END_TEST

#define UNION_TYPE_SMOKE_START(_type_, fields_number) \
do{ \
	struct metac_type *type = METAC_TYPE(_type_); \
	_type_ struct_; \
	fail_unless(metac_type_union_member_count(type) == (fields_number), \
			"metac_type_union_member_count incorrect value for " #_type_ ": %d", fields_number); \


#define UNION_TYPE_SMOKE_MEMBER(_member_name_) \
	do { \
		struct metac_type_member_info member_info; \
		struct metac_type * member_type = metac_type_union_member_by_name(type, #_member_name_); \
		fail_unless(member_type != NULL, "couldn't find member " #_member_name_); \
		fail_unless(metac_type_member_info(member_type, &member_info) == 0, "failed to get member info for " #_member_name_); \
		/* check name*/\
		fail_unless(member_info.name != NULL, "member_info.name is NULL"); \
		fail_unless(strcmp(member_info.name, #_member_name_) == 0, "member_info.name is %s instead of %s", \
				member_info.name, #_member_name_); \
		/* check type*/\
		fail_unless(member_info.type != NULL, "member_info.type is NULL"); \
		/* check offset */\
		fail_unless(((char*)&(struct_._member_name_)) - ((char*)&(struct_)) == \
				(member_info.p_data_member_location == NULL?0:*member_info.p_data_member_location),\
				"data_member_location is incorrect for " #_member_name_ ": %d instead of %d", \
				(int)(member_info.p_data_member_location == NULL?0:*member_info.p_data_member_location),\
				(int)(((char*)&(struct_._member_name_)) - ((char*)&(struct_)))); \
		\
	} while(0)

#define UNION_TYPE_SMOKE_END \
	mark_point(); \
} while(0)

#define STRUCT_TYPE_SMOKE_START(_type_, fields_number) \
do{ \
	struct metac_type *type = METAC_TYPE(_type_); \
	_type_ struct_; \
	fail_unless(metac_type_structure_member_count(type) == (fields_number), \
			"metac_type_structure_member_count incorrect value for " #_type_ ": %d", fields_number); \


#define STRUCT_TYPE_SMOKE_MEMBER(_member_name_) \
	do { \
		struct metac_type_member_info member_info; \
		struct metac_type * member_type = metac_type_structure_member_by_name(type, #_member_name_); \
		fail_unless(member_type != NULL, "couldn't find member " #_member_name_); \
		fail_unless(metac_type_member_info(member_type, &member_info) == 0, "failed to get member info for " #_member_name_); \
		/* check name*/\
		fail_unless(member_info.name != NULL, "member_info.name is NULL"); \
		fail_unless(strcmp(member_info.name, #_member_name_) == 0, "member_info.name is %s instead of %s", \
				member_info.name, #_member_name_); \
		/* check type*/\
		fail_unless(member_info.type != NULL, "member_info.type is NULL"); \
		/* check offset */\
		fail_unless(member_info.p_data_member_location != NULL, "member_info.p_data_member_location is NULL"); \
		fail_unless(((char*)&(struct_._member_name_)) - ((char*)&(struct_)) == *member_info.p_data_member_location,\
				"data_member_location is incorrect for " #_member_name_ ": %d instead of %d", \
				(int)*member_info.p_data_member_location,\
				(int)(((char*)&(struct_._member_name_)) - ((char*)&(struct_)))); \
		\
	} while(0)

#define STRUCT_TYPE_SMOKE_MEMBER_BIT_FIELD(_member_name_) \
	do { \
		int i; \
		int bit_size = 0; \
		unsigned int mask; /*will take number of bits*/ \
		struct metac_type_member_info member_info; \
		struct metac_type * member_type = metac_type_structure_member_by_name(type, #_member_name_); \
		fail_unless(member_type != NULL, "couldn't find member " #_member_name_); \
		fail_unless(metac_type_member_info(member_type, &member_info) == 0, "failed to get member info for " #_member_name_); \
		/* check name*/\
		fail_unless(member_info.name != NULL, "member_info.name is NULL"); \
		fail_unless(strcmp(member_info.name, #_member_name_) == 0, "member_info.name is %s instead of %s", \
				member_info.name, #_member_name_); \
		/* check type*/\
		fail_unless(member_info.type != NULL, "member_info.type is NULL"); \
		/* check offset */\
		fail_unless(member_info.p_data_member_location != NULL, "member_info.p_data_member_location is NULL"); \
		/* check bit_size*/ \
		memset(&struct_, 0xff, sizeof(struct_)); \
		mask = struct_._member_name_; \
		struct_.heightValidated = 0; \
		\
		while (mask != 0) { \
			mask >>= 1; \
			bit_size++; \
		} \
		fail_unless(*member_info.p_bit_size == bit_size, "bit_size is incorrect %d instead of %d", \
				(int)*member_info.p_bit_size, (int)bit_size); \
		/*TODO: make check for bit_offset and data_member_location (for big/little endian) - depends on implementation*/ \
	} while(0)

#define STRUCT_TYPE_SMOKE_END \
	mark_point(); \
} while(0)

START_TEST(struct_type_smoke) {
	UNION_TYPE_SMOKE_START(union_t, 2)
		UNION_TYPE_SMOKE_MEMBER(d);
		UNION_TYPE_SMOKE_MEMBER(f);
	UNION_TYPE_SMOKE_END;

	STRUCT_TYPE_SMOKE_START(struct_t, 2)
		STRUCT_TYPE_SMOKE_MEMBER(heightValidated);
		STRUCT_TYPE_SMOKE_MEMBER(widthValidated);
	STRUCT_TYPE_SMOKE_END;

	STRUCT_TYPE_SMOKE_START(bit_fields_t, 2)
		STRUCT_TYPE_SMOKE_MEMBER_BIT_FIELD(heightValidated);
		STRUCT_TYPE_SMOKE_MEMBER_BIT_FIELD(widthValidated);
	STRUCT_TYPE_SMOKE_END;

}END_TEST

#define FUNC_TYPE_SMOKE(_type_, _s_type_) \
do{ \
	struct metac_type *type = METAC_TYPE(_type_); \
	struct metac_type *typedef_skip_type = metac_type_typedef_skip(type); \
	\
	fail_unless(metac_type(typedef_skip_type) == _s_type_, "must be " #_s_type_ ", but it's 0x%x", (int)metac_type(typedef_skip_type)); \
	fail_unless(metac_type_byte_size(type) == sizeof(_type_), \
			"metac_type_byte_size returned for " #_type_" incorrect value %d instead of %d", metac_type_byte_size(type), sizeof(_type_)); \
	fail_unless(strcmp(metac_type_name(type), #_type_ ) == 0, "type name returned '%s' instead of '" #_type_ "'", metac_type_name(type));\
	\
	mark_point(); \
} while(0)

START_TEST(func_type_smoke) {
	FUNC_TYPE_SMOKE(func_t, DW_TAG_subprogram);
}END_TEST

/* arrays */
typedef char_t char_array_t[0];
METAC_EXPORT_TYPE(char_array_t);


START_TEST(array_type_smoke) {
	/* test for array with bounds */
	do {
		char_array5_t reference_object;
		struct metac_type_element_info element_info;
		struct metac_type_subrange_info subrange_info;
		struct metac_type *type = METAC_TYPE(char_array5_t);
		struct metac_type *subrange_type;
		unsigned int subrange_count = metac_type_array_subrange_count(type);
		fail_unless(subrange_count > 0, "subrange_count must be more than 0");
		subrange_type = metac_type_array_subrange(type, subrange_count - 1);
		fail_unless(subrange_type != NULL, "subrange must be not NULL");
		fail_unless(metac_type_subrange_info(subrange_type, &subrange_info) == 0, "metac_type_subrange_info returned error");
		fail_unless(subrange_info.p_upper_bound != NULL, "subrange_info.p_upper_bound must present");
		fail_unless(*(subrange_info.p_upper_bound) == (sizeof(reference_object)/sizeof(reference_object[0]) - 1) ,
				"incorrect upper bound %d instead of %d", (int)*(subrange_info.p_upper_bound), (int)(sizeof(reference_object)/sizeof(reference_object[0]) - 1));

		fail_unless(metac_type_array_element_type(type) == METAC_TYPE(char_t), "metac_type_array_element_type returned incorrect pointer");

		fail_unless(metac_type_array_element_info(type, *(subrange_info.p_upper_bound), &element_info) == 0, "metac_type_array_element_info returned error");
		fail_unless((((char*)&reference_object[*(subrange_info.p_upper_bound)]) - ((char*)&reference_object[0]) == element_info.element_location),
				"incorrect element location %d instead of %d",
				(int)element_info.element_location,
				(int)(((char*)(&reference_object[*(subrange_info.p_upper_bound)])) - ((char*)(&reference_object[0]))));

		fail_unless(metac_type_array_element_info(type, *(subrange_info.p_upper_bound) + 1, &element_info) != 0, "metac_type_array_element_info must fail");
	}while(0);
	/* test for array without bounds */
	do {
		char_array_t reference_object;
		struct metac_type_element_info element_info;
		struct metac_type_subrange_info subrange_info;
		struct metac_type *type = METAC_TYPE(char_array_t);
		struct metac_type *subrange_type;
		unsigned int subrange_count = metac_type_array_subrange_count(type);
		fail_unless(subrange_count > 0, "subrange_count must be more than 0");
		subrange_type = metac_type_array_subrange(type, subrange_count - 1);
		fail_unless(subrange_type != NULL, "subrange must be not NULL");
		fail_unless(metac_type_subrange_info(subrange_type, &subrange_info) == 0, "metac_type_subrange_info returned error");
		fail_unless(subrange_info.p_upper_bound == NULL, "subrange_info.p_upper_bound must not present");
		fail_unless(subrange_info.p_lower_bound == NULL, "subrange_info.p_lower_bound must not present");
		fail_unless(metac_type_array_element_type(type) == METAC_TYPE(char_t), "metac_type_array_element_type returned incorrect pointer");
		fail_unless(metac_type_array_element_info(type, 0, &element_info) == 0, "metac_type_array_element_info must fail");
	}while(0);

}END_TEST

#define ENUM_TYPE_SMOKE(enum_type, et_name, expected_e_info_values) \
do { \
	unsigned int i; \
	struct metac_type_enumeration_type_info et_info; \
	struct metac_type_enumerator_info e_info; \
    \
	fail_unless(metac_type_enumeration_type_info(enum_type, &et_info) == 0, "metac_type_enumeration_type_info: expected success"); \
    \
	fail_unless((et_info.name == NULL && et_name == NULL) || strcmp(et_info.name, et_name) == 0, "invalid name %s instead of %s", et_info.name, et_name); \
	fail_unless(et_info.byte_size > 0 && et_info.byte_size <= 8, "expected byte_size in the range(0,8] instead of %u", et_info.byte_size); \
	fail_unless(et_info.enumerators_count == sizeof(expected_e_info_values)/sizeof(struct metac_type_enumerator_info), "children number must be %u instead of %u", \
			sizeof(expected_e_info_values)/sizeof(struct metac_type_enumerator_info), et_info.enumerators_count); \
    \
	for (i = 0; i < et_info.enumerators_count; i++) { \
		fail_unless(metac_type_enumeration_type_enumerator_info(enum_type, i, &e_info) == 0, "expected success"); \
		fail_unless(strcmp(e_info.name, expected_e_info_values[i].name) == 0, "expected %s instead of %s", expected_e_info_values[i].name, e_info.name); \
		fail_unless((int)e_info.const_value == expected_e_info_values[i].const_value, "expected %d instead of %d", expected_e_info_values[i].const_value, e_info.const_value); \
	}\
}while(0)


START_TEST(enum_type_smoke) {
	struct metac_type *enum_type = METAC_TYPE(enum_t);
	struct metac_type_enumerator_info enum_type_expected_e_info_values[] = {
			{.name = "_eZero", .const_value = 0},
			{.name = "_eOne", .const_value = 1},
			{.name = "_eTen", .const_value = 10},
			{.name = "_eEleven", .const_value = 11},
			{.name = "_eTwelve", .const_value = 12},
	};
	struct metac_type *anon_enum_type = METAC_TYPE(anon_enum_t);
	struct metac_type_enumerator_info anon_enum_type_expected_e_info_values[] = {
			{.name = "aeMinus", .const_value = -1},
			{.name = "aeZero", .const_value = 0},
			{.name = "aeOne", .const_value = 1},
			{.name = "aeTen", .const_value = 10},
			{.name = "aeEleven", .const_value = 11},
			{.name = "aeTwelve", .const_value = 12},
	};

	ENUM_TYPE_SMOKE(enum_type, "_enum_", enum_type_expected_e_info_values);
	ENUM_TYPE_SMOKE(anon_enum_type, NULL, anon_enum_type_expected_e_info_values);

}END_TEST

int main(void){
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(type_smoke){
					ADD_TEST(general_type_smoke);
					ADD_TEST(struct_type_smoke);
					ADD_TEST(func_type_smoke);
					ADD_TEST(array_type_smoke);
					ADD_TEST(enum_type_smoke);
				}END_CASE
			);
		}END_SUITE
	);
}

