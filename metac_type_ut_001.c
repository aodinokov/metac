/*
 * metac_ut_001.c
 *
 *  Created on: Oct 3, 2015
 *      Author: mralex
 */


#include "check_ext.h"
#include "metac_type.h"

#include <dlfcn.h>
#include <complex.h>	/*complex*/

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
/*****************************************************************************/
METAC_DECLARE_EXTERN_TYPES_ARRAY;
METAC_DECLARE_EXTERN_OBJECTS_ARRAY;

#define GENERAL_TYPE_CHECK_INIT(_type_) do { \
		/*need to use the type, in the opposite case we'll not get type definintion in DWARF*/ \
		_type_*ptr = NULL; \
	}while(0)
#define GENERAL_TYPE_CHECK_BYTE_SIZE(_type_) do { \
		struct metac_type *type = &METAC_TYPE_NAME(_type_);\
		fail_unless(metac_type_byte_size(type) == sizeof(_type_), "metac_type_byte_size returned incorrect value for " #_type_); \
	}while(0)
#define GENERAL_TYPE_CHECK_NAME(_type_) do { \
		struct metac_type *type = &METAC_TYPE_NAME(_type_);\
		char * type_name = metac_type_name(type); \
		/*workaround for long int and short int - get only first word*/ \
		char * type_name_copy = strdup((type_name!=NULL)?type_name:""), \
			 * tmp = strchr(type_name_copy, ' '); \
		if (tmp != NULL)*tmp = '\0';  \
		fail_unless(strcmp(type_name_copy, #_type_ ) == 0, "type name returned '%s' instead of '" #_type_ "'", metac_type_name(type));\
		free(type_name_copy);\
	}while(0)
#define GENERAL_TYPE_CHECK_ACCESS_BY_NAME(_type_) do { \
		struct metac_type *type = &METAC_TYPE_NAME(_type_);\
		struct metac_type *type_by_name = metac_type_by_name(&METAC_TYPES_ARRAY, #_type_);\
		fail_unless(type_by_name == type, "metac_type_by_name returned incorrect value %p", type_by_name);\
	}while(0)
#define GENERAL_TYPE_CHECK_ID(_type_, _id_) do { \
		struct metac_type *type = &METAC_TYPE_NAME(_type_);\
		fail_unless(type->id == _id_, "ID: must be " #_id_ ", but it's 0x%x", (int)type->id); \
	}while(0)
#define GENERAL_TYPE_CHECK_NOT_TYPEDEF_ID(_type_, _id_) do { \
		struct metac_type *type = &METAC_TYPE_NAME(_type_);\
		struct metac_type *typedef_skip_type = metac_type_typedef_skip(type); \
		fail_unless(typedef_skip_type->id == _id_, "NOT_TYPEDEF_ID: must be " #_id_ ", but it's 0x%x", (int)typedef_skip_type->id); \
	}while(0)
#define GENERAL_TYPE_CHECK_SPEC(_type_, _spec_key_, _spec_val_) do { \
		struct metac_type *type = &METAC_TYPE_NAME(_type_);\
		char *spec_key = _spec_key_; \
		char *_spec_val = _spec_val_; \
		if (spec_key) { \
			const char* spec_val = metac_type_specification(type, _spec_key_); \
			fail_unless(spec_val != NULL, "metac_type_specification returned NULL unexpectidly"); \
			fail_unless(strcmp(_spec_val, spec_val) == 0, "got incorrect specification"); \
		} \
	}while(0)
#define GENERAL_TYPE_CHECK(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_) \
	mark_point(); \
	GENERAL_TYPE_CHECK_INIT(_type_); \
	GENERAL_TYPE_CHECK_BYTE_SIZE(_type_); \
	GENERAL_TYPE_CHECK_NAME(_type_); \
	GENERAL_TYPE_CHECK_ACCESS_BY_NAME(_type_); \
	GENERAL_TYPE_CHECK_ID(_type_, _id_); \
	GENERAL_TYPE_CHECK_NOT_TYPEDEF_ID(_type_, _n_td_id_);\
	GENERAL_TYPE_CHECK_SPEC(_type_, _spec_key_, _spec_val_);


/*****************************************************************************/
#define BASE_TYPE_CHECK GENERAL_TYPE_CHECK

METAC_TYPE_GENERATE(char);
METAC_TYPE_GENERATE(short);
METAC_TYPE_GENERATE(int);
METAC_TYPE_GENERATE(long);
METAC_TYPE_GENERATE(float);
METAC_TYPE_GENERATE(double);

START_TEST(base_types_no_typedef) {
	BASE_TYPE_CHECK(char, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(short, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(int, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(long, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(float, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(double, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
}END_TEST
/*****************************************************************************/
typedef char char_t;
typedef signed char schar_t;
typedef unsigned char uchar_t;

METAC_TYPE_GENERATE(char_t);
METAC_TYPE_GENERATE(schar_t);
METAC_TYPE_GENERATE(uchar_t);

typedef short short_t;
typedef short int shortint_t;
typedef signed short sshort_t;
typedef signed short int sshortint_t;
typedef unsigned short ushort_t;
typedef unsigned short int ushortint_t;

METAC_TYPE_GENERATE(short_t);
METAC_TYPE_GENERATE(shortint_t);
METAC_TYPE_GENERATE(sshort_t);
METAC_TYPE_GENERATE(sshortint_t);
METAC_TYPE_GENERATE(ushort_t);
METAC_TYPE_GENERATE(ushortint_t);

typedef int int_t;
typedef signed signed_t;
typedef signed int sint_t;
typedef unsigned unsigned_t;
typedef unsigned int uint_t;

METAC_TYPE_GENERATE(int_t);
METAC_TYPE_GENERATE(signed_t);
METAC_TYPE_GENERATE(sint_t);
METAC_TYPE_GENERATE(unsigned_t);
METAC_TYPE_GENERATE(uint_t);

typedef long long_t;
typedef long int longint_t;
typedef signed long slong_t;
typedef signed long int slongint_t;
typedef unsigned long ulong_t;
typedef unsigned long int ulongint_t;
METAC_TYPE_GENERATE(long_t);
METAC_TYPE_GENERATE(longint_t);
METAC_TYPE_GENERATE(slong_t);
METAC_TYPE_GENERATE(slongint_t);
METAC_TYPE_GENERATE(ulong_t);
METAC_TYPE_GENERATE(ulongint_t);

typedef long long llong_t;
typedef long long int llongint_t;
typedef signed long long sllong_t;
typedef signed long long int sllongint_t;
typedef unsigned long long ullong_t;
typedef unsigned long long int ullongint_t;
METAC_TYPE_GENERATE(llong_t);
METAC_TYPE_GENERATE(llongint_t);
METAC_TYPE_GENERATE(sllong_t);
METAC_TYPE_GENERATE(sllongint_t);
METAC_TYPE_GENERATE(ullong_t);
METAC_TYPE_GENERATE(ullongint_t);

typedef float float_t;
METAC_TYPE_GENERATE(float_t);
typedef double double_t;
METAC_TYPE_GENERATE(double_t);
typedef long double ldouble_t;
METAC_TYPE_GENERATE(ldouble_t);
typedef double complex doublecomplex_t;
METAC_TYPE_GENERATE(doublecomplex_t);

START_TEST(base_types_with_typedef) {
	BASE_TYPE_CHECK(char, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(short, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(int, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(long, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(float, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(double, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);

	BASE_TYPE_CHECK(char_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(schar_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(uchar_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);

	BASE_TYPE_CHECK(short_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(shortint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(sshort_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(sshortint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(ushort_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(ushortint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);

	BASE_TYPE_CHECK(int_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(signed_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(sint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(unsigned_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(uint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);

	BASE_TYPE_CHECK(long_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(longint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(slong_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(slongint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(ulong_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(ulongint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);

	BASE_TYPE_CHECK(llong_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(llongint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(sllong_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(sllongint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(ullong_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(ullongint_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);

	BASE_TYPE_CHECK(float_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(double_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(ldouble_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);

	BASE_TYPE_CHECK(doublecomplex_t, DW_TAG_typedef, DW_TAG_base_type, NULL, NULL);
}END_TEST

/*****************************************************************************/
#define POINTER_TYPE_CHECK GENERAL_TYPE_CHECK
typedef void* voidptr_t;
METAC_TYPE_GENERATE(voidptr_t);
typedef void** voidptrptr_t;
METAC_TYPE_GENERATE(voidptrptr_t);
typedef char* charptr_t;
METAC_TYPE_GENERATE(charptr_t);

START_TEST(pointers_with_typedef) {
	POINTER_TYPE_CHECK(voidptr_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
	POINTER_TYPE_CHECK(voidptrptr_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
	POINTER_TYPE_CHECK(charptr_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
}END_TEST

/*****************************************************************************/
#define ENUM_TYPE_CHECK_NAME(_type_, _expected_name_, _expected_name_skip_typedef_) do {\
	struct metac_type *type = &METAC_TYPE_NAME(_type_);\
	struct metac_type *typedef_skip_type = metac_type_typedef_skip(type); \
	char * expected_name_skip_typedef = _expected_name_skip_typedef_ /*can be NULL for anonymous enums*/; \
	char * type_name = metac_type_name(type); \
	char * type_name_skip_typedef = metac_type_name(typedef_skip_type); \
	fail_unless(strcmp(type_name, _expected_name_) == 0, \
			"check_name: %s instead of %s", type_name, _expected_name_); \
	if (expected_name_skip_typedef != NULL) \
		fail_unless(strcmp(type_name_skip_typedef, expected_name_skip_typedef) == 0, \
				"check_name_skip_typedef:  %s instead of %s", type_name_skip_typedef, expected_name_skip_typedef); \
	else \
		fail_unless(type_name_skip_typedef == NULL, \
				"check_name_skip_typedef: Got non-NULL string instead of NULL"); \
	}while(0)
#define ENUM_TYPE_CHECK_VALS(_type_, _expected_vals_...) do {\
		struct metac_type *type = metac_type_typedef_skip(&METAC_TYPE_NAME(_type_)); \
		static struct metac_type_enumerator_info expected_values[] = _expected_vals_; \
		int i = 0; \
		fail_unless(type->enumeration_type_info.byte_size == sizeof(_type_), \
				"enum byte_size %d donsn't match sizeof value %d", \
				type->enumeration_type_info.byte_size, \
				sizeof(_type_)); \
		while (expected_values[i].name != NULL) { \
			metac_const_value_t const_value = 0; \
			metac_name_t res_name = metac_type_enumeration_type_get_name(type, expected_values[i].const_value); \
			fail_unless(metac_type_enumeration_type_get_value(type, expected_values[i].name, &const_value) == 0, \
					"metac_type_enumeration_type_get_value failed for %s", expected_values[i].name); \
			fail_unless(expected_values[i].const_value == const_value, "got incorrect value");\
			fail_unless(strcmp(res_name, expected_values[i].name) == 0, "got incorrect name");\
			++i;\
		}\
		fail_unless(i == type->enumeration_type_info.enumerators_count, "incorrect enumerators_count"); \
	}while(0)
#define ENUM_TYPE_CHECK(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_, _expected_name_, _expected_name_skip_typedef_, _expected_vals_...) \
		GENERAL_TYPE_CHECK(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_); \
		ENUM_TYPE_CHECK_NAME(_type_, _expected_name_, _expected_name_skip_typedef_); \
		ENUM_TYPE_CHECK_VALS(_type_, _expected_vals_)

typedef enum _enum_{
	_eZero = 0,
	_eOne,
	_eTen = 10,
	_eEleven,
	_eTwelve,
}enum_t;
METAC_TYPE_GENERATE(enum_t);
typedef enum{
	aeMinus = -1,
	aeZero = 0,
	aeOne = 1,
	aeTen = 10,
	aeEleven,
	aeTwelve,
}anon_enum_t;
METAC_TYPE_GENERATE(anon_enum_t);
typedef enum __attribute__((packed, aligned(16)))_aligned_enum_{
	al_eZero = 0,
	al_eOne,
	al_eTen = 10,
	al_eEleven,
	al_eTwelve,
}aligned_enum_t;
METAC_TYPE_GENERATE(aligned_enum_t);

START_TEST(enums_with_typedef) {
	ENUM_TYPE_CHECK(enum_t, DW_TAG_typedef, DW_TAG_enumeration_type, NULL, NULL, "enum_t", "_enum_", {
		{.name = "_eZero", .const_value = 0},
		{.name = "_eOne", .const_value = 1},
		{.name = "_eTen", .const_value = 10},
		{.name = "_eEleven", .const_value = 11},
		{.name = "_eTwelve", .const_value = 12},
		{.name = NULL},
	});
	ENUM_TYPE_CHECK(anon_enum_t, DW_TAG_typedef, DW_TAG_enumeration_type, NULL, NULL, "anon_enum_t", NULL, {
		{.name = "aeMinus", .const_value = -1},
		{.name = "aeZero", .const_value = 0},
		{.name = "aeOne", .const_value = 1},
		{.name = "aeTen", .const_value = 10},
		{.name = "aeEleven", .const_value = 11},
		{.name = "aeTwelve", .const_value = 12},
		{.name = NULL},
	});
	ENUM_TYPE_CHECK(aligned_enum_t, DW_TAG_typedef, DW_TAG_enumeration_type, NULL, NULL, "aligned_enum_t", "_aligned_enum_",{
		{.name = "al_eZero", .const_value = 0},
		{.name = "al_eOne", .const_value = 1},
		{.name = "al_eTen", .const_value = 10},
		{.name = "al_eEleven", .const_value = 11},
		{.name = "al_eTwelve", .const_value = 12},
		{.name = NULL},
	});
}END_TEST
/*****************************************************************************/
#define ARRAY_TYPE_CHECK_BEGIN_(_type_, _init_...) do { \
		static _type_ obj = _init_; \
		struct metac_type *type = metac_type_typedef_skip(&METAC_TYPE_NAME(_type_));
#define _ARRAY_TYPE_CHECK_VALS(_element_type_name_, _expected_is_flexible_) do { \
			char * element_type_name = _element_type_name_; \
			int is_flexible = _expected_is_flexible_; \
			fail_unless(is_flexible == type->array_type_info.is_flexible, "is_flexible flag doesn't match"); \
			fail_unless(strcmp(metac_type_name(type->array_type_info.type), element_type_name) == 0, \
				"is_flexible flag doesn't match: %s, expected %s", \
				metac_type_name(type->array_type_info.type),\
				element_type_name); \
		}while(0)
struct _subrange_info { int res; metac_count_t count; };
#define _ARRAY_TYPE_CHECK_SUBRANGES(_expected_subranges_count, _expected_subranges_...) do { \
			metac_num_t i; \
			metac_num_t subranges_count = _expected_subranges_count; \
			static struct _subrange_info expected_subranges[] = _expected_subranges_; \
			fail_unless(subranges_count == type->array_type_info.subranges_count, \
				"Subranges count doesn't match: %d, expected %d", type->array_type_info.subranges_count, subranges_count); \
			for (i = 0; i < subranges_count; i++) {\
				metac_count_t count; \
				int res = metac_type_array_subrange_count(type, i, &count);\
				fail_unless(res == expected_subranges[i].res, \
					"metac_type_array_subrange_count returned unexpected value for i %d: %d, expected %d", \
					(int)i, \
					(int)res, \
					(int)expected_subranges[i].res); \
				if (res == 0) \
					fail_unless(count == expected_subranges[i].count, "unexpected count for i %d", (int)i); \
			} \
		}while(0)
#define _delta(_obj_, _postfix_) \
	(((void*)&_obj_ _postfix_) - (void*)&_obj_)
#define _ARRAY_TYPE_CHECK_LOCATION(_obj_indx_, _should_warn_, _n_indx_, indx...) do { \
			metac_num_t subranges_count = _n_indx_;\
			int should_warn = _should_warn_; \
			metac_num_t subranges[] = indx; \
			metac_data_member_location_t data_member_location = 0;\
			int call_res = metac_type_array_member_location(type, \
					subranges_count, subranges, &data_member_location); \
			fail_unless(call_res >= 0, "metac_type_array_member_location failed"); \
			fail_unless(should_warn == call_res, "warning doesn't work as expected"); \
			fail_unless(((int)_delta(obj, _obj_indx_)) == (int)data_member_location, "_delta is different: %x and got %x", \
					(int)_delta(obj, _obj_indx_), (int)data_member_location); \
		}while(0)
#define ARRAY_TYPE_CHECK_END \
	}while(0)

#define ARRAY_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_, _init_...) \
	GENERAL_TYPE_CHECK(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_); \
	ARRAY_TYPE_CHECK_BEGIN_(_type_, _init_)

typedef char_t char_array_t[0];
METAC_TYPE_GENERATE(char_array_t);
typedef char_t char_array5_t[5];
METAC_TYPE_GENERATE(char_array5_t);
typedef char _2darray_t[2][3];
METAC_TYPE_GENERATE(_2darray_t);
typedef char _3darray_t[5][4][3];
METAC_TYPE_GENERATE(_3darray_t);
typedef char _3darray1_t[5][0][3];
METAC_TYPE_GENERATE(_3darray1_t);

#define _BUG_ZERO_LEN_IS_FLEXIBLE_ 1
#ifdef __clang__
/*clang 3.5.0 has this issue, but 3.9.0 - doesn't */
#if __clang_major__ >= 3 && __clang_minor__ >= 9 && __clang_patchlevel__ >=0
#undef  _BUG_ZERO_LEN_IS_FLEXIBLE_
#define _BUG_ZERO_LEN_IS_FLEXIBLE_ 0
#endif
#endif

START_TEST(array_with_typedef) {
	ARRAY_TYPE_CHECK_BEGIN(char_array_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {});
	_ARRAY_TYPE_CHECK_VALS("char_t", _BUG_ZERO_LEN_IS_FLEXIBLE_); /*this array looks like a flexible for DWARF*/
	_ARRAY_TYPE_CHECK_SUBRANGES(1, {{1, 0}, });
	ARRAY_TYPE_CHECK_END;
	ARRAY_TYPE_CHECK_BEGIN(char_array5_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {});
	_ARRAY_TYPE_CHECK_VALS("char_t", 0);
	_ARRAY_TYPE_CHECK_SUBRANGES(1, {{0, 5}, });
	_ARRAY_TYPE_CHECK_LOCATION([1], 0, 1, {1, });
	ARRAY_TYPE_CHECK_END;
	ARRAY_TYPE_CHECK_BEGIN(_2darray_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {});
	_ARRAY_TYPE_CHECK_VALS("char", 0);
	_ARRAY_TYPE_CHECK_SUBRANGES(2, {{0, 2}, {0, 3}, } );
	_ARRAY_TYPE_CHECK_LOCATION([0], 0, 1, {0, });
	_ARRAY_TYPE_CHECK_LOCATION([1], 0, 1, {1, });
	_ARRAY_TYPE_CHECK_LOCATION([2], 1, 1, {2, });
	_ARRAY_TYPE_CHECK_LOCATION([0][0], 0, 2, {0, 0, });
	_ARRAY_TYPE_CHECK_LOCATION([0][1], 0, 2, {0, 1, });
	_ARRAY_TYPE_CHECK_LOCATION([0][2], 0, 2, {0, 2, });
	_ARRAY_TYPE_CHECK_LOCATION([0][3], 1, 2, {0, 3, });
	_ARRAY_TYPE_CHECK_LOCATION([1][0], 0, 2, {1, 0, });
	_ARRAY_TYPE_CHECK_LOCATION([1][1], 0, 2, {1, 1, });
	_ARRAY_TYPE_CHECK_LOCATION([1][2], 0, 2, {1, 2, });
	_ARRAY_TYPE_CHECK_LOCATION([1][3], 1, 2, {1, 3, });
	_ARRAY_TYPE_CHECK_LOCATION([2][0], 1, 2, {2, 0, });
	_ARRAY_TYPE_CHECK_LOCATION([2][1], 1, 2, {2, 1, });
	_ARRAY_TYPE_CHECK_LOCATION([2][2], 1, 2, {2, 2, });
	_ARRAY_TYPE_CHECK_LOCATION([3][2], 1, 2, {3, 2, });
	ARRAY_TYPE_CHECK_END;
	ARRAY_TYPE_CHECK_BEGIN(_3darray_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {});
	_ARRAY_TYPE_CHECK_VALS("char", 0);
	_ARRAY_TYPE_CHECK_SUBRANGES(3, {{0, 5}, {0, 4}, {0, 3}, });
	ARRAY_TYPE_CHECK_END;
	ARRAY_TYPE_CHECK_BEGIN(_3darray1_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {});
	_ARRAY_TYPE_CHECK_VALS("char", _BUG_ZERO_LEN_IS_FLEXIBLE_); /*this array looks like a flexible for DWARF*/
	_ARRAY_TYPE_CHECK_SUBRANGES(3, {{0, 5}, {1, 0}, {0, 3}, });
	_ARRAY_TYPE_CHECK_LOCATION([4][2][4], 2, 3, {4, 2, 4, });
	ARRAY_TYPE_CHECK_END;
}END_TEST

/*****************************************************************************/

/* unions */
typedef union _union_{
	int d;
	char f;
}union_t;
METAC_TYPE_GENERATE(union_t);

/* struct */
typedef struct _struct_
{
	unsigned int widthValidated;
	unsigned int heightValidated;
}struct_t;
METAC_TYPE_GENERATE(struct_t);

/* bit fields */
typedef struct _bit_fields_
{
  unsigned int widthValidated : 9;
  unsigned int heightValidated : 12;
}bit_fields_t;
METAC_TYPE_GENERATE(bit_fields_t);

typedef struct _bit_fields_for_longer_than32_bit
{
	unsigned int widthValidated : 9;
	unsigned int heightValidated : 12;
	int heightValidated1 : 30;
	llongint_t heightValidated2 : 63;
}bit_fields_for_longer_than32_bit_t;
METAC_TYPE_GENERATE(bit_fields_for_longer_than32_bit_t);

struct _struct_with_flexible_array_and_len {
	int array_len;
	char array[];
};
typedef struct _struct_with_struct_with_flexible_array_and_len{
	int before_struct;
	struct _struct_with_flexible_array_and_len str1;
}struct_with_struct_with_flexible_array_and_len_t;
METAC_TYPE_GENERATE(struct_with_struct_with_flexible_array_and_len_t);
METAC_TYPE_SPECIFICATION_BEGIN(struct_with_struct_with_flexible_array_and_len_t)
_METAC_TYPE_SPECIFICATION("discrimitator_name", "array_len")
METAC_TYPE_SPECIFICATION_END

/*TODO: some combinations??? */

/* function ptr */
typedef int_t (*func_ptr_t)(bit_fields_t *arg);
METAC_TYPE_GENERATE(func_ptr_t);

/* function */
typedef bit_fields_t * p_bit_fields_t;
METAC_TYPE_GENERATE(p_bit_fields_t);
int_t func_t(p_bit_fields_t arg) {if (arg)return 1; return 0;}
METAC_FUNCTION(func_t);

typedef const char * cchar_t;
METAC_TYPE_GENERATE(cchar_t);
void func_printf(cchar_t format, ...){return;}
METAC_FUNCTION(func_printf);

START_TEST(general_type_smoke) {

	GENERAL_TYPE_CHECK(union_t, DW_TAG_typedef, DW_TAG_union_type, NULL, NULL);

	GENERAL_TYPE_CHECK(struct_t, DW_TAG_typedef, DW_TAG_structure_type, NULL, NULL);
	GENERAL_TYPE_CHECK(bit_fields_t, DW_TAG_typedef, DW_TAG_structure_type, NULL, NULL);
	GENERAL_TYPE_CHECK(bit_fields_for_longer_than32_bit_t, DW_TAG_typedef, DW_TAG_structure_type, NULL, NULL);
	GENERAL_TYPE_CHECK(struct_with_struct_with_flexible_array_and_len_t, DW_TAG_typedef, DW_TAG_structure_type, "discrimitator_name", "array_len");

	GENERAL_TYPE_CHECK(p_bit_fields_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
	GENERAL_TYPE_CHECK(cchar_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);

	GENERAL_TYPE_CHECK(func_ptr_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
}END_TEST

//#define UNION_TYPE_SMOKE_START(_type_, fields_number) \
//do{ \
//	struct metac_type *type = &METAC_TYPE_NAME(_type_); \
//	_type_ struct_; \
//	struct metac_type_union_info _info_;\
//	fail_unless(metac_type_union_info(type, &_info_) == 0, "get info returned error");\
//	fail_unless(_info_.members_count == (fields_number), \
//			"metac_type_union_member_count incorrect value for " #_type_ ": %d", fields_number); \
//
//
//#define UNION_TYPE_SMOKE_MEMBER(_member_name_) \
//	do { \
//		struct metac_type_member_info member_info; \
//		int i = metac_type_child_id_by_name(type, #_member_name_); \
//		fail_unless(i >= 0, "couldn't find member " #_member_name_); \
//		fail_unless(metac_type_union_member_info(type, (unsigned int)i, &member_info) == 0, "failed to get member info for " #_member_name_); \
//		/* check name*/\
//		fail_unless(member_info.name != NULL, "member_info.name is NULL"); \
//		fail_unless(strcmp(member_info.name, #_member_name_) == 0, "member_info.name is %s instead of %s", \
//				member_info.name, #_member_name_); \
//		/* check type*/\
//		fail_unless(member_info.type != NULL, "member_info.type is NULL"); \
//		/* check offset */\
//		fail_unless(((char*)&(struct_._member_name_)) - ((char*)&(struct_)) == \
//				(member_info.p_data_member_location == NULL?0:*member_info.p_data_member_location),\
//				"data_member_location is incorrect for " #_member_name_ ": %d instead of %d", \
//				(int)(member_info.p_data_member_location == NULL?0:*member_info.p_data_member_location),\
//				(int)(((char*)&(struct_._member_name_)) - ((char*)&(struct_)))); \
//		\
//	} while(0)
//
//#define UNION_TYPE_SMOKE_END \
//	mark_point(); \
//} while(0)
//
//#define STRUCT_TYPE_SMOKE_START(_type_, fields_number) \
//do{ \
//	struct metac_type *type = &METAC_TYPE_NAME(_type_); \
//	_type_ struct_; \
//	struct metac_type_structure_info _info_;\
//	fail_unless(metac_type_structure_info(type, &_info_) == 0, "get info returned error");\
//	fail_unless(_info_.members_count == (fields_number), \
//			"metac_type_structure_member_count incorrect value for " #_type_ ": %d", fields_number); \
//
//#define STRUCT_TYPE_SMOKE_MEMBER(_member_name_) \
//	do { \
//		struct metac_type_member_info member_info; \
//		int i = metac_type_child_id_by_name(type, #_member_name_); \
//		fail_unless(i >= 0, "couldn't find member " #_member_name_); \
//		fail_unless(metac_type_structure_member_info(type, (unsigned int)i, &member_info) == 0, "failed to get member info for " #_member_name_); \
//		/* check name*/ \
//		fail_unless(member_info.name != NULL, "member_info.name is NULL"); \
//		fail_unless(strcmp(member_info.name, #_member_name_) == 0, "member_info.name is %s instead of %s", \
//				member_info.name, #_member_name_); \
//		/* check type*/\
//		fail_unless(member_info.type != NULL, "member_info.type is NULL"); \
//		/* check offset */\
//		fail_unless(member_info.p_data_member_location != NULL, "member_info.p_data_member_location is NULL"); \
//		fail_unless(((char*)&(struct_._member_name_)) - ((char*)&(struct_)) == *member_info.p_data_member_location,\
//				"data_member_location is incorrect for " #_member_name_ ": %d instead of %d", \
//				(int)*member_info.p_data_member_location,\
//				(int)(((char*)&(struct_._member_name_)) - ((char*)&(struct_)))); \
//		\
//	} while(0)
//
//#define STRUCT_TYPE_SMOKE_MEMBER_BIT_FIELD(_member_name_) \
//	do { \
//		int bit_size = 0; \
//		unsigned int mask; /*will take number of bits*/ \
//		struct metac_type_member_info member_info; \
//		int i = metac_type_child_id_by_name(type, #_member_name_); \
//		fail_unless(i >= 0, "couldn't find member " #_member_name_); \
//		fail_unless(metac_type_structure_member_info(type, (unsigned int)i, &member_info) == 0, "failed to get member info for " #_member_name_); \
//		/* check name*/\
//		fail_unless(member_info.name != NULL, "member_info.name is NULL"); \
//		fail_unless(strcmp(member_info.name, #_member_name_) == 0, "member_info.name is %s instead of %s", \
//				member_info.name, #_member_name_); \
//		/* check type*/\
//		fail_unless(member_info.type != NULL, "member_info.type is NULL"); \
//		/* check offset */\
//		fail_unless(member_info.p_data_member_location != NULL, "member_info.p_data_member_location is NULL"); \
//		/* check bit_size*/ \
//		memset(&struct_, 0xff, sizeof(struct_)); \
//		mask = struct_._member_name_; \
//		struct_.heightValidated = 0; \
//		\
//		while (mask != 0) { \
//			mask >>= 1; \
//			bit_size++; \
//		} \
//		fail_unless(*member_info.p_bit_size == bit_size, "bit_size is incorrect %d instead of %d", \
//				(int)*member_info.p_bit_size, (int)bit_size); \
//		/*TODO: make check for bit_offset and data_member_location (for big/little endian) - depends on implementation*/ \
//	} while(0)
//
//#define STRUCT_TYPE_SMOKE_END \
//	mark_point(); \
//} while(0)
//
//START_TEST(struct_type_smoke) {
//	UNION_TYPE_SMOKE_START(union_t, 2)
//		UNION_TYPE_SMOKE_MEMBER(d);
//		UNION_TYPE_SMOKE_MEMBER(f);
//	UNION_TYPE_SMOKE_END;
//
//	STRUCT_TYPE_SMOKE_START(struct_t, 2)
//		STRUCT_TYPE_SMOKE_MEMBER(heightValidated);
//		STRUCT_TYPE_SMOKE_MEMBER(widthValidated);
//	STRUCT_TYPE_SMOKE_END;
//
//	STRUCT_TYPE_SMOKE_START(bit_fields_t, 2)
//		STRUCT_TYPE_SMOKE_MEMBER_BIT_FIELD(heightValidated);
//		STRUCT_TYPE_SMOKE_MEMBER_BIT_FIELD(widthValidated);
//	STRUCT_TYPE_SMOKE_END;
//}END_TEST
//
//#define FUNC_TYPE_SMOKE(_type_, _s_type_, expected_return_type, expected_parameter_info_values) \
//do{ \
//	unsigned int i; \
//	struct metac_type *type = &METAC_TYPE_NAME(_type_); \
//	struct metac_type *typedef_skip_type = metac_type_typedef_skip(type); \
//	struct metac_type_subprogram_info s_info; \
//	struct metac_type_parameter_info p_info; \
//	struct metac_object * p_object; \
//	\
//	fail_unless(metac_type_id(typedef_skip_type) == _s_type_, "must be " #_s_type_ ", but it's 0x%x", (int)metac_type_id(typedef_skip_type)); \
//	fail_unless(metac_type_byte_size(type) == sizeof(_type_), \
//			"metac_type_byte_size returned for " #_type_" incorrect value %d instead of %d", metac_type_byte_size(type), sizeof(_type_)); \
//	fail_unless(strcmp(metac_type_name(type), #_type_ ) == 0, "type name returned '%s' instead of '" #_type_ "'", metac_type_name(type));\
//	\
//	mark_point(); \
//	\
//	fail_unless(metac_type_subprogram_info(type, &s_info) == 0, "metac_type_subprogram_info: expected success"); \
//	\
//	fail_unless(strcmp(s_info.name, #_type_) == 0, "invalid name %s instead of %s", s_info.name, #_type_); \
//	fail_unless(s_info.type == expected_return_type, "not expected return type %p instead of %p", s_info.type, expected_return_type); \
//	fail_unless(s_info.parameters_count == sizeof(expected_parameter_info_values)/sizeof(struct metac_type_parameter_info), "params number must be %u instead of %u", \
//			sizeof(expected_parameter_info_values)/sizeof(struct metac_type_parameter_info), s_info.parameters_count); \
//	\
//	for (i = 0; i < s_info.parameters_count; i++) { \
//		fail_unless(metac_type_subprogram_parameter_info(type, i, &p_info) == 0, "expected success"); \
//		fail_unless(p_info.unspecified_parameters == expected_parameter_info_values[i].unspecified_parameters, "expected %d instead of %d", expected_parameter_info_values[i].unspecified_parameters, p_info.unspecified_parameters); \
//		if (p_info.unspecified_parameters == 0) { \
//			fail_unless(strcmp(p_info.name, expected_parameter_info_values[i].name) == 0, "expected %s instead of %s", expected_parameter_info_values[i].name, p_info.name); \
//			fail_unless(p_info.type == expected_parameter_info_values[i].type, "wrong parameter type: expected %p instead of %p", expected_parameter_info_values[i].type, p_info.type); \
//		}\
//	}\
//	\
//	mark_point(); \
//	\
//	p_object = metac_object_by_name(&METAC_OBJECTS_ARRAY, #_type_);\
//	fail_unless(p_object != NULL, "metac_object_by_name returned incorrect value %p", p_object);\
//	fail_unless(p_object->type == type, "p_object_info.type must be == type");\
//	fail_unless(p_object->flexible_part_byte_size == 0, "p_object_info.flexible_part_byte_size must be == 0");\
//	mark_point(); \
//} while(0)
//
//START_TEST(func_type_smoke) {
//	struct metac_type_parameter_info func_t_expected_parameter_info_values[] = {
//			{.unspecified_parameters = 0, .name = "arg", .type = &METAC_TYPE_NAME(p_bit_fields_t)},
//	};
//	struct metac_type_parameter_info func_printf_expected_parameter_info_values[] = {
//			{.unspecified_parameters = 0, .name = "format", .type = &METAC_TYPE_NAME(cchar_t)},
//#if !defined(__clang__) || (__clang_major__ == 3 && __clang_minor__ > 4)
///* Workaround: looks like clang3.4 doesn't have ability to show if function has unspecified parameter. See https://travis-ci.org/aodinokov/metac/jobs/184151833#L472*/
//			{.unspecified_parameters = 1, .name = NULL, .type = NULL},
//#endif
//	};
//
//	FUNC_TYPE_SMOKE(func_t, DW_TAG_subprogram, &METAC_TYPE_NAME(int_t), func_t_expected_parameter_info_values);
//	FUNC_TYPE_SMOKE(func_printf, DW_TAG_subprogram, NULL, func_printf_expected_parameter_info_values);
//
//}END_TEST


START_TEST(metac_array_symbols) {
	mark_point();
	mark_point();
	void * handle = dlopen(NULL, RTLD_NOW);
	fail_unless(handle != NULL, "dlopen failed");
	void * types_array = dlsym(handle, METAC_TYPES_ARRAY_SYMBOL);
	void * objects_array = dlsym(handle, METAC_OBJECTS_ARRAY_SYMBOL);
	dlclose(handle);
	fail_unless(types_array == &METAC_TYPES_ARRAY, "can't find correct %s: %p", METAC_TYPES_ARRAY_SYMBOL, types_array);
	fail_unless(objects_array == &METAC_OBJECTS_ARRAY, "can't find correct %s: %p", METAC_OBJECTS_ARRAY_SYMBOL, objects_array);
}END_TEST
//
///*serialization - move to another file*/
//struct metac_object * metac_json2object(struct metac_type * mtype, char *string);
//
//typedef struct _struct1_
//{
//  unsigned int x;
//  unsigned int y;
//}struct1_t;
//
//typedef struct _struct2_
//{
//	struct1_t * str1;
//}struct2_t;
//METAC_TYPE_GENERATE(struct2_t);
//
//START_TEST(metac_json_deserialization) {
//	struct metac_object * res;
//	do {
//		char * pres;
//		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(char), "\"c\"")) != NULL, "metac_json2object returned NULL");
//		pres = (char*)res->ptr;
//		fail_unless(*pres == 'c', "unexpected data");
//		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
//	}while(0);
//	mark_point();
//	do {
//		int *pres;
//		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(int), "7")) != NULL, "metac_json2object returned NULL");
//		pres = (int*)res->ptr;
//		fail_unless(*pres == 7, "unexpected data");
//		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
//	}while(0);
//	mark_point();
//	do {
//		int_t *pres;
//		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(int_t), "7777")) != NULL, "metac_json2object returned NULL");
//		pres = (int_t*)res->ptr;
//		fail_unless(*pres == 7777, "unexpected data");
//		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
//	}while(0);
//	mark_point();
//
//	do {
//		enum_t *pres;
//		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(enum_t), "\"_eOne\"")) != NULL, "metac_json2object returned NULL");
//		pres = (enum_t*)res->ptr;
//		fail_unless(*pres == _eOne, "unexpected data");
//		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
//	}while(0);
//	mark_point();
//	/*nedative fail_unless((res = metac_json2object(&METAC_TYPE_NAME(enum_t), "\"x_eOne\"")) != NULL, "metac_json2object returned NULL");*/
//
//	do {
//		char_array5_t *pres;
//		char_array5_t eres = {'a', 'b', 'c', 'd', 'e'};
//		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(char_array5_t), "[\"a\", \"b\",\"c\",\"d\",\"e\",]")) != NULL,
//				"metac_json2object returned NULL");
//		pres = (char_array5_t*)res->ptr;
//		fail_unless(memcmp(pres, &eres, sizeof(eres)) == 0, "unexpected data");
//		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
//	}while(0);
//	mark_point();
//
///*
//	typedef struct _bit_fields_
//	{
//	  unsigned int widthValidated : 9;
//	  unsigned int heightValidated : 12;
//	}bit_fields_t;
//	METAC_TYPE_GENERATE(bit_fields_t);
//*/
//
//
//	do {
//		int i;
//		unsigned char * p;
//		bit_fields_t *pres;
//		bit_fields_t eres = {.widthValidated = 6, .heightValidated = 100};
//		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(bit_fields_t), "{\"widthValidated\": 6, \"heightValidated\": 100}")) != NULL,
//				"metac_json2object returned NULL");
//		pres = (bit_fields_t*)res->ptr;
///*
//printf("result:\n");
//p = (unsigned char *) pres;
//for (i=0; i<sizeof(bit_fields_t); i++){
//printf("%02x ", (int)*p);
//p++;
//}
//printf("\n");
//printf("expected:\n");
//p = (unsigned char *) &eres;
//for (i=0; i<sizeof(bit_fields_t); i++){
//printf("%02x ", (int)*p);
//p++;
//}
//printf("\n");
//*/
//
//		fail_unless(
//				pres->widthValidated == eres.widthValidated &&
//				pres->heightValidated == eres.heightValidated, "unexpected data");
//		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
//	}while(0);
//	mark_point();
//
///*
//typedef struct _bit_fields_for_longer_than32_bit
//{
//  unsigned int widthValidated : 9;
//  unsigned int heightValidated : 12;
//  int heightValidated1 : 30;
//  llongint_t heightValidated2 : 63;
//}bit_fields_for_longer_than32_bit_t;
//METAC_TYPE_GENERATE(bit_fields_for_longer_than32_bit_t);
// */
//	do {
//		int i;
//		unsigned char * p;
//		bit_fields_for_longer_than32_bit_t *pres;
//		bit_fields_for_longer_than32_bit_t eres = {
//				.widthValidated = 6,
//				.heightValidated = 100,
//				.heightValidated1 = -1,
//				.heightValidated2 = 10000000};
//		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(bit_fields_for_longer_than32_bit_t),
//				"{\"widthValidated\": 6, \"heightValidated\": 100, \"heightValidated1\": -1, \"heightValidated2\": 10000000}")) != NULL,
//				"metac_json2object returned NULL");
//		pres = (bit_fields_for_longer_than32_bit_t*)res->ptr;
////printf("result:\n");
////p = (unsigned char *) pres;
////for (i=0; i<sizeof(bit_fields_for_longer_than32_bit_t); i++){
////printf("%02x ", (int)*p);
////p++;
////}
////printf("\n");
////printf("expected:\n");
////p = (unsigned char *) &eres;
////for (i=0; i<sizeof(bit_fields_for_longer_than32_bit_t); i++){
////printf("%02x ", (int)*p);
////p++;
////}
////printf("\n");
//		fail_unless(
//				pres->widthValidated == eres.widthValidated &&
//				pres->heightValidated == eres.heightValidated /*&&
//				pres->heightValidated1 == eres.heightValidated1 &&
//				pres->heightValidated2 == eres.heightValidated2*/
//				, "unexpected data");
//		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
//	}while(0);
//	mark_point();
//
//
//
//	/*nedative fail_unless((res = metac_json2object(&METAC_TYPE_NAME(char_array5_t), "[\"a\", \"b\",\"c\",\"d\",\"e\",\"f\",]")) != NULL,
//	 * 		"metac_json2object returned NULL");*/
//	GENERAL_TYPE_CHECK(struct2_t, DW_TAG_structure_type);
//	do {
//		struct2_t *pres;
//		struct1_t str1 = {.x = 1, .y = 8};
//		struct2_t eres = {.str1 = &str1};
//
//		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(struct2_t), "{\"str1\":{\"x\": 1, \"y\": 8}}")) != NULL,
//				"metac_json2object returned NULL");
//		pres = (struct2_t*)res->ptr;
//		fail_unless(pres->str1 != NULL, "pointer wasn't initialized");
//		fail_unless(
//				pres->str1->x == eres.str1->x &&
//				pres->str1->y == eres.str1->y, "unexpected data");
//		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
//	}while(0);
//	mark_point();
//
//}END_TEST

int main(void){
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(type_smoke){
					ADD_TEST(base_types_no_typedef);
					ADD_TEST(base_types_with_typedef);
					ADD_TEST(pointers_with_typedef);
					ADD_TEST(enums_with_typedef);
					ADD_TEST(general_type_smoke);
					ADD_TEST(array_with_typedef);
//					ADD_TEST(struct_type_smoke);
//					ADD_TEST(func_type_smoke);
//					ADD_TEST(array_type_smoke);
//					ADD_TEST(enum_type_smoke);
					ADD_TEST(metac_array_symbols);
//					ADD_TEST(metac_json_deserialization);
				}END_CASE
			);
		}END_SUITE
	);
}

