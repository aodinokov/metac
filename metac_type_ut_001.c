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

#define DUMP_MEM(_text_, _start_, _size_) \
	do { \
		int i; \
		unsigned char * p = (unsigned char *)_start_; \
		printf("%s", _text_); \
		for (i=0; i<(_size_); i++) { \
			printf("%02x ", (int)*p++); \
		} \
		printf("\n"); \
	}while(0)


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
 */

/**
 * Know issues and workarounds:
 **/
/* 1. signed/unsinged <char|short|int|long>, long long, void*(all pointers and arrays) won't work
 * because they are not one-work types.
 * use typedefs to export that types via METAC - fixed in metac.awk
 */
/* 2. looks like clang3.4 doesn't have ability to show if function has unspecified parameter.
 * See https://travis-ci.org/aodinokov/metac/jobs/184151833#L472
 */
#if !defined(__clang__) || (__clang_major__ == 3 && __clang_minor__ > 4)
#define _BUG_NO_USPECIFIED_PARAMETER_ 0
#else
#define _BUG_NO_USPECIFIED_PARAMETER_ 1
#endif

/* 3. Some compilers generate dwarf data without count/upper/lower_bound for arrays with len=0. It's a bug.
 * gcc and clang 3.5.0 has this issue, but 3.9.0 - doesn't
 */
#if defined(__clang__) && __clang_major__ >= 3 && __clang_minor__ >= 9 && __clang_patchlevel__ >=0
#define _BUG_ZERO_LEN_IS_FLEXIBLE_ 0
#else
#define _BUG_ZERO_LEN_IS_FLEXIBLE_ 1
#endif


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

START_TEST(base_types_ut) {
	BASE_TYPE_CHECK(char, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(short, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(int, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(long, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(float, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);
	BASE_TYPE_CHECK(double, DW_TAG_base_type, DW_TAG_base_type, NULL, NULL);

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
struct _incomplete_;
typedef struct _incomplete_ incomplete_t;
typedef incomplete_t * p_incomplete_t;
METAC_TYPE_GENERATE(p_incomplete_t);
typedef const char * cchar_t;
METAC_TYPE_GENERATE(cchar_t);
typedef int_t (*func_ptr_t)(doublecomplex_t *arg);
METAC_TYPE_GENERATE(func_ptr_t);

START_TEST(pointers_ut) {
	POINTER_TYPE_CHECK(voidptr_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
	POINTER_TYPE_CHECK(voidptrptr_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
	POINTER_TYPE_CHECK(charptr_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
	POINTER_TYPE_CHECK(p_incomplete_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
	POINTER_TYPE_CHECK(cchar_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
	POINTER_TYPE_CHECK(func_ptr_t, DW_TAG_typedef, DW_TAG_pointer_type, NULL, NULL);
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

START_TEST(enums_ut) {
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
#define _array_delta(_obj_, _postfix_) (((void*)&_obj_ _postfix_) - (void*)&_obj_)
#define _ARRAY_TYPE_CHECK_LOCATION(_obj_indx_, _should_warn_, _n_indx_, indx...) do { \
			metac_num_t subranges_count = _n_indx_;\
			int should_warn = _should_warn_; \
			metac_num_t subranges[] = indx; \
			metac_data_member_location_t data_member_location = 0;\
			int call_res = metac_type_array_member_location(type, \
					subranges_count, subranges, &data_member_location); \
			fail_unless(call_res >= 0, "metac_type_array_member_location failed"); \
			fail_unless(should_warn == call_res, "warning doesn't work as expected"); \
			fail_unless(((int)_array_delta(obj, _obj_indx_)) == (int)data_member_location, "_delta is different: %x and got %x", \
					(int)_array_delta(obj, _obj_indx_), (int)data_member_location); \
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

START_TEST(arrays_ut) {
	ARRAY_TYPE_CHECK_BEGIN(char_array_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {}) {
		_ARRAY_TYPE_CHECK_VALS("char_t", _BUG_ZERO_LEN_IS_FLEXIBLE_); /*this array looks like a flexible for DWARF*/
		_ARRAY_TYPE_CHECK_SUBRANGES(1, {{_BUG_ZERO_LEN_IS_FLEXIBLE_, 0}, });
	}ARRAY_TYPE_CHECK_END;
	ARRAY_TYPE_CHECK_BEGIN(char_array5_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {}) {
		_ARRAY_TYPE_CHECK_VALS("char_t", 0);
		_ARRAY_TYPE_CHECK_SUBRANGES(1, {{0, 5}, });
		_ARRAY_TYPE_CHECK_LOCATION([1], 0, 1, {1, });
	}ARRAY_TYPE_CHECK_END;
	ARRAY_TYPE_CHECK_BEGIN(_2darray_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {}) {
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
	}ARRAY_TYPE_CHECK_END;
	ARRAY_TYPE_CHECK_BEGIN(_3darray_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {}) {
		_ARRAY_TYPE_CHECK_VALS("char", 0);
		_ARRAY_TYPE_CHECK_SUBRANGES(3, {{0, 5}, {0, 4}, {0, 3}, });
		_ARRAY_TYPE_CHECK_LOCATION([1][0][0], 0, 3, {1, 0, 0, });
		_ARRAY_TYPE_CHECK_LOCATION([1][0][1], 0, 3, {1, 0, 1, });
		_ARRAY_TYPE_CHECK_LOCATION([1][3][2], 0, 3, {1, 3, 2, });
	}ARRAY_TYPE_CHECK_END;
	ARRAY_TYPE_CHECK_BEGIN(_3darray1_t, DW_TAG_typedef, DW_TAG_array_type, NULL, NULL, {}) {
		_ARRAY_TYPE_CHECK_VALS("char", _BUG_ZERO_LEN_IS_FLEXIBLE_); /*this array looks like a flexible for DWARF*/
		_ARRAY_TYPE_CHECK_SUBRANGES(3, {{0, 5}, {_BUG_ZERO_LEN_IS_FLEXIBLE_, 0}, {0, 3}, });
		_ARRAY_TYPE_CHECK_LOCATION([4][2][4], 2, 3, {4, 2, 4, });
	}ARRAY_TYPE_CHECK_END;
}END_TEST

/*****************************************************************************/
#define STRUCT_UNION_TYPE_CHECK_BEGIN_(_type_, _init_...) do { \
		static _type_ obj = _init_; \
		struct metac_type *type = metac_type_typedef_skip(&METAC_TYPE_NAME(_type_));
#define _STRUCT_UNION_TYPE_CHECK_BYTESIZE(_type_info_) do { \
			metac_byte_size_t byte_size = sizeof(obj); \
			fail_unless(byte_size == type->_type_info_.byte_size, "byte_size doesn't match"); \
		}while(0)
struct _member_info { metac_name_t name; metac_data_member_location_t location; int flag;};
#define _struct_union_delta(_obj_, _postfix_) (((void*)&_obj_ _postfix_) - (void*)&_obj_)
#define _struct_union_bit_location(_field_name)({ \
	/* bit-fields: specific only for structures, but this method can work for non-bit-fields */ \
	metac_data_member_location_t res = 0; \
	memset(&obj, 0xff, sizeof(obj)); \
	obj. _field_name = 0; \
	DUMP_MEM("after : ",&obj, sizeof(obj)); \
	for (res = 0; res < sizeof(obj); res++) \
		if (((unsigned char*)&obj)[res] != 0xff) break; \
	printf("res: %d\n", (int)res); \
	res;\
})
#define __STRUCT_UNION_TYPE_CHECK_MEMBER(_member_name_) \
	{.flag = 1, .name = #_member_name_ , .location = _struct_union_delta(obj, . _member_name_), }
#define __STRUCT_UNION_TYPE_CHECK_ANON_MEMBER(_first_member_name_) \
	{.flag = 1, .name = METAC_ANON_MEMBER_NAME, .location = _struct_union_delta(obj, . _first_member_name_), }
#define __STRUCT_UNION_TYPE_CHECK_MEMBER_BITFIELD(_member_name_) \
	{.flag = 2, .name = #_member_name_, .location = _struct_union_bit_location(_member_name_), }
#define _STRUCT_UNION_TYPE_CHECK_MEMBERS(_type_info_, _expected_members_count, _expected_members_...) do { \
			metac_num_t i; \
			metac_num_t members_count = _expected_members_count; \
			struct _member_info expected_members[] = _expected_members_; \
			fail_unless(members_count == type->_type_info_.members_count, "members_count doesn't match"); \
			for (i = 0; i < members_count; i++) {\
				fail_unless(strcmp(type->_type_info_.members[i].name, expected_members[i].name) == 0, "name mismatch");\
				fail_unless(type->_type_info_.members[i].type != NULL, "type is NULL");\
				switch(expected_members[i].flag) {\
				case 1: \
				fail_unless(type->_type_info_.members[i].data_member_location == expected_members[i].location, "incorrect location"); \
				break; \
				case 2: \
				fail_unless(type->_type_info_.members[i].p_bit_size != NULL, "bit_size must be set"); \
				fail_unless(expected_members[i].location >= type->_type_info_.members[i].data_member_location && \
						expected_members[i].location < type->_type_info_.members[i].data_member_location + metac_type_byte_size(type->_type_info_.members[i].type), \
					"incorrect bit location: member %s, got %x, expected in range [%x, %x)", \
					expected_members[i].name,\
					type->_type_info_.members[i].data_member_location, \
					expected_members[i].location, \
					expected_members[i].location < type->_type_info_.members[i].data_member_location + metac_type_byte_size(type->_type_info_.members[i].type)); \
				break; \
				} \
			} \
		}while(0)
#define STRUCT_UNION_TYPE_CHECK_END \
	}while(0)
/*****************************************************************************/
#define STRUCT_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_, _init_...) \
	GENERAL_TYPE_CHECK(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_); \
	STRUCT_UNION_TYPE_CHECK_BEGIN_(_type_, _init_)
#define _STRUCT_TYPE_CHECK_BYTESIZE \
	_STRUCT_UNION_TYPE_CHECK_BYTESIZE(structure_type_info)
#define __STRUCT_TYPE_CHECK_MEMBER __STRUCT_UNION_TYPE_CHECK_MEMBER
#define __STRUCT_TYPE_CHECK_ANON_MEMBER __STRUCT_UNION_TYPE_CHECK_ANON_MEMBER
#define __STRUCT_TYPE_CHECK_MEMBER_BITFIELD __STRUCT_UNION_TYPE_CHECK_MEMBER_BITFIELD
#define _STRUCT_TYPE_CHECK_MEMBERS(_expected_members_count, _expected_members_...) \
		_STRUCT_UNION_TYPE_CHECK_MEMBERS(structure_type_info, _expected_members_count, _expected_members_)
#define STRUCT_TYPE_CHECK_END \
	STRUCT_UNION_TYPE_CHECK_END

typedef struct _struct_ {
	unsigned int widthValidated;
	unsigned int heightValidated;
}struct_t;
METAC_TYPE_GENERATE(struct_t);
typedef struct _bit_fields_ {
	unsigned int widthValidated : 9;
	unsigned int heightValidated : 12;
}bit_fields_t;
METAC_TYPE_GENERATE(bit_fields_t);
typedef struct _bit_fields_for_longer_than32_bit {
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
typedef struct _struct_with_struct_with_flexible_array_and_len {
	int before_struct;
	struct _struct_with_flexible_array_and_len str1;
}struct_with_struct_with_flexible_array_and_len_t;
METAC_TYPE_GENERATE(struct_with_struct_with_flexible_array_and_len_t);
METAC_TYPE_SPECIFICATION_BEGIN(struct_with_struct_with_flexible_array_and_len_t)
_METAC_TYPE_SPECIFICATION("discrimitator_name", "array_len")
METAC_TYPE_SPECIFICATION_END
typedef struct {
	struct {
		int a;
		int b;
	};
	struct {
		long c;
		double d;
	};
	union {
		int x;
		long y;
	};
	int e;
}anon_struct_with_anon_elements;
METAC_TYPE_GENERATE(anon_struct_with_anon_elements);

START_TEST(structs_ut) {
	STRUCT_TYPE_CHECK_BEGIN(struct_t, DW_TAG_typedef, DW_TAG_structure_type, NULL, NULL, {}) {
		_STRUCT_TYPE_CHECK_BYTESIZE;
		_STRUCT_TYPE_CHECK_MEMBERS(2, {
		__STRUCT_TYPE_CHECK_MEMBER(widthValidated),
		__STRUCT_TYPE_CHECK_MEMBER(heightValidated),
		});
	}STRUCT_TYPE_CHECK_END;
	STRUCT_TYPE_CHECK_BEGIN(bit_fields_t, DW_TAG_typedef, DW_TAG_structure_type, NULL, NULL, {}) {
		_STRUCT_TYPE_CHECK_BYTESIZE;
		_STRUCT_TYPE_CHECK_MEMBERS(2, {
		__STRUCT_TYPE_CHECK_MEMBER_BITFIELD(widthValidated),
		__STRUCT_TYPE_CHECK_MEMBER_BITFIELD(heightValidated),
		});
	}STRUCT_TYPE_CHECK_END;
	STRUCT_TYPE_CHECK_BEGIN(bit_fields_for_longer_than32_bit_t, DW_TAG_typedef, DW_TAG_structure_type, NULL, NULL, {}) {
		_STRUCT_TYPE_CHECK_BYTESIZE;
		_STRUCT_TYPE_CHECK_MEMBERS(4, {
		__STRUCT_TYPE_CHECK_MEMBER_BITFIELD(widthValidated),
		__STRUCT_TYPE_CHECK_MEMBER_BITFIELD(heightValidated),
		__STRUCT_TYPE_CHECK_MEMBER_BITFIELD(heightValidated1),
		__STRUCT_TYPE_CHECK_MEMBER_BITFIELD(heightValidated2),
		});
	}STRUCT_TYPE_CHECK_END;
	STRUCT_TYPE_CHECK_BEGIN(struct_with_struct_with_flexible_array_and_len_t, DW_TAG_typedef, DW_TAG_structure_type,
			"discrimitator_name", "array_len", {}) {
		_STRUCT_TYPE_CHECK_BYTESIZE;
		_STRUCT_TYPE_CHECK_MEMBERS(2, {
		__STRUCT_TYPE_CHECK_MEMBER(before_struct),
		__STRUCT_TYPE_CHECK_MEMBER(str1),
		});
	}STRUCT_TYPE_CHECK_END;
	STRUCT_TYPE_CHECK_BEGIN(anon_struct_with_anon_elements, DW_TAG_typedef, DW_TAG_structure_type, NULL, NULL, {}) {
		_STRUCT_TYPE_CHECK_BYTESIZE;
		_STRUCT_TYPE_CHECK_MEMBERS(4, {
		__STRUCT_TYPE_CHECK_ANON_MEMBER(a),
		__STRUCT_TYPE_CHECK_ANON_MEMBER(c),
		__STRUCT_TYPE_CHECK_ANON_MEMBER(x),
		__STRUCT_TYPE_CHECK_MEMBER(e),
		});
	}STRUCT_TYPE_CHECK_END;
}END_TEST
/*****************************************************************************/
#define UNION_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_, _init_...) \
	GENERAL_TYPE_CHECK(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_); \
	STRUCT_UNION_TYPE_CHECK_BEGIN_(_type_, _init_)
#define _UNION_TYPE_CHECK_BYTESIZE \
	_STRUCT_UNION_TYPE_CHECK_BYTESIZE(union_type_info)
#define __UNION_TYPE_CHECK_MEMBER __STRUCT_UNION_TYPE_CHECK_MEMBER
#define __UNION_TYPE_CHECK_ANON_MEMBER __STRUCT_UNION_TYPE_CHECK_ANON_MEMBER
#define __UNION_TYPE_CHECK_MEMBER_BITFIELD __STRUCT_UNION_TYPE_CHECK_MEMBER_BITFIELD
#define _UNION_TYPE_CHECK_MEMBERS(_expected_members_count, _expected_members_...) \
		_STRUCT_UNION_TYPE_CHECK_MEMBERS(union_type_info, _expected_members_count, _expected_members_)
#define UNION_TYPE_CHECK_END \
	STRUCT_UNION_TYPE_CHECK_END

typedef union _union_{
	int d;
	char f;
}union_t;
METAC_TYPE_GENERATE(union_t);
typedef union _union_anon_{
	struct {
		char a;
		int b;
	};
	long c;
	struct {
		int d;
		char f;
	};
}union_anon_t;
METAC_TYPE_GENERATE(union_anon_t);

START_TEST(unions_ut) {
	UNION_TYPE_CHECK_BEGIN(union_t, DW_TAG_typedef, DW_TAG_union_type, NULL, NULL, {}) {
		_UNION_TYPE_CHECK_BYTESIZE;
		_UNION_TYPE_CHECK_MEMBERS(2, {
		__UNION_TYPE_CHECK_MEMBER(d),
		__UNION_TYPE_CHECK_MEMBER(f),
		});
	}UNION_TYPE_CHECK_END;
	UNION_TYPE_CHECK_BEGIN(union_anon_t, DW_TAG_typedef, DW_TAG_union_type, NULL, NULL, {}) {
		_UNION_TYPE_CHECK_BYTESIZE;
		_UNION_TYPE_CHECK_MEMBERS(3, {
		__UNION_TYPE_CHECK_ANON_MEMBER(a),
		__UNION_TYPE_CHECK_MEMBER(c),
		__UNION_TYPE_CHECK_ANON_MEMBER(d),
		});
	}UNION_TYPE_CHECK_END;
}END_TEST
/*****************************************************************************/
#define FUNCTION_CHECK_BEGIN_(_type_) do { \
		char *type_name = #_type_; \
		struct metac_type *type = metac_type_typedef_skip(&METAC_TYPE_NAME(_type_));
#define _FUNCTION_CHECK_GLOBALLY_ACCESSIBLE do { \
			struct metac_object * p_object; \
			mark_point(); \
			p_object = metac_object_by_name(&METAC_OBJECTS_ARRAY, type_name); \
			fail_unless(p_object != NULL, "metac_object_by_name returned incorrect value %p", p_object);\
			fail_unless(p_object->type == type, "p_object_info.type must be == type");\
			fail_unless(p_object->flexible_part_byte_size == 0, "p_object_info.flexible_part_byte_size must be == 0");\
		}while(0)
#define _FUNCTION_CHECK_RETURN_TYPE(_return_type_name_) do { \
			char* return_type_name = _return_type_name_; \
			if (return_type_name == NULL) {\
				fail_unless(type->subprogram_info.type == NULL, "return type doesn't correlate with expected"); \
			} else { \
				fail_unless(metac_type_name(type->subprogram_info.type) != NULL, "can't get name - (PS: will not work for arrays)"); \
				fail_unless(strcmp(return_type_name, metac_type_name(type->subprogram_info.type)) == 0, "type name doesn't match"); \
			} \
		}while(0)
#define _FUNCTION_CHECK_PARAMS(_bug_with_unspecified_parameters_, _parameters_count_, _expected_parameters_...) do { \
			metac_num_t i; \
			int bug_with_unspecified_parameters = _bug_with_unspecified_parameters_; \
			metac_num_t parameters_count = _parameters_count_; \
			struct metac_type_subprogram_parameter expected_parameters[] = _expected_parameters_; \
			fail_unless(parameters_count == type->subprogram_info.parameters_count + bug_with_unspecified_parameters, \
				"parameters_count doesn't match"); \
			for (i = 0; i < type->subprogram_info.parameters_count; i++) { \
				fail_unless(type->subprogram_info.parameters[i].unspecified_parameters == expected_parameters[i].unspecified_parameters, \
					"unspecified_parameters parameter mismatch"); \
				if (expected_parameters[i].unspecified_parameters == 0) { \
					fail_unless(strcmp(type->subprogram_info.parameters[i].name, expected_parameters[i].name) == 0, "name mismatch");\
				} \
			} \
		}while(0)

#define FUNCTION_TYPE_CHECK_END \
	}while(0)

#define FUNCTION_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_, _spec_key_, _spec_val_) \
	mark_point(); \
	GENERAL_TYPE_CHECK_BYTE_SIZE(_type_); \
	GENERAL_TYPE_CHECK_NAME(_type_); \
	GENERAL_TYPE_CHECK_ACCESS_BY_NAME(_type_); \
	GENERAL_TYPE_CHECK_ID(_type_, _id_); \
	GENERAL_TYPE_CHECK_NOT_TYPEDEF_ID(_type_, _n_td_id_);\
	GENERAL_TYPE_CHECK_SPEC(_type_, _spec_key_, _spec_val_); \
	FUNCTION_CHECK_BEGIN_(_type_)

typedef bit_fields_t * p_bit_fields_t;
int_t func_t(p_bit_fields_t arg) {if (arg)return 1; return 0;}
METAC_FUNCTION(func_t);
void func_printf(cchar_t format, ...){return;}
METAC_FUNCTION(func_printf);

START_TEST(funtions_ut){
	FUNCTION_TYPE_CHECK_BEGIN(func_t, DW_TAG_subprogram, DW_TAG_subprogram, NULL, NULL) {
		_FUNCTION_CHECK_GLOBALLY_ACCESSIBLE;
		_FUNCTION_CHECK_RETURN_TYPE("int_t");
		_FUNCTION_CHECK_PARAMS(_BUG_NO_USPECIFIED_PARAMETER_, 1, {
			{.name = "arg", },
		});
	}FUNCTION_TYPE_CHECK_END;
	FUNCTION_TYPE_CHECK_BEGIN(func_printf, DW_TAG_subprogram, DW_TAG_subprogram, NULL, NULL) {
		_FUNCTION_CHECK_GLOBALLY_ACCESSIBLE;
		_FUNCTION_CHECK_RETURN_TYPE(NULL);
		_FUNCTION_CHECK_PARAMS(_BUG_NO_USPECIFIED_PARAMETER_, 2, {
			{.name = "format", },
			{.unspecified_parameters = 1, },
		});
	}FUNCTION_TYPE_CHECK_END;
}END_TEST
/*****************************************************************************/
START_TEST(metac_array_symbols) {
	mark_point();
	mark_point();
	void * handle = dlopen(NULL, RTLD_NOW);
	fail_unless(handle != NULL, "dlopen failed");
	void * types_array = dlsym(handle, METAC_TYPES_ARRAY_SYMBOL);
	void * objects_array = dlsym(handle, METAC_OBJECTS_ARRAY_SYMBOL);
	dlclose(handle);
	fail_unless(types_array == &METAC_TYPES_ARRAY,
			"can't find correct %s: %p", METAC_TYPES_ARRAY_SYMBOL, types_array);
	fail_unless(objects_array == &METAC_OBJECTS_ARRAY,
			"can't find correct %s: %p", METAC_OBJECTS_ARRAY_SYMBOL, objects_array);
}END_TEST
/*****************************************************************************/
int main(void){
	printf("bug_zero_len_is_flexible %d\n", _BUG_ZERO_LEN_IS_FLEXIBLE_);
	printf("bug_with_unspecified_parameters %d\n", _BUG_NO_USPECIFIED_PARAMETER_);
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(type_smoke){
					ADD_TEST(base_types_ut);
					ADD_TEST(pointers_ut);
					ADD_TEST(enums_ut);
					ADD_TEST(arrays_ut);
					ADD_TEST(structs_ut);
					ADD_TEST(unions_ut);
					ADD_TEST(funtions_ut);
					ADD_TEST(metac_array_symbols);
				}END_CASE
			);
		}END_SUITE
	);
}

