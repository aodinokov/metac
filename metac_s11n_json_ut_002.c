/*
 * metac_s11n_json_ut_002.c
 *
 *  Created on: Nov 5, 2017
 *      Author: mralex
 */

#include <stdint.h>	/*INT32_MIN, INT32_MAX*/
#include <stdio.h>	/*sprintf*/
#include <string.h> /*strdupa*/

#include "metac_s11n_json.h"

#include "check_ext.h"

/*
 * UT helper macros - > to debug
 */
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

/**
 * json0->cdata0->json1->cdata1 (json0 and 1 will not be identical, there are many ways to set json)
 */
#define JSON_POSITIVE_BEGIN_WITH_LEN(_static_, _type_, _json0_, _json1_, _expected_data_len_, _expected_data_...) \
	do { \
		int i; \
		_static_ _type_ expected_data = _expected_data_; \
		struct metac_object * cdata, * _cdata[2]; \
		char * json, * _json[2] = {_json0_, _json1_}; \
		char * json1, *_json1; \
		_cdata[0] = metac_json2object(&METAC_TYPE_NAME(_type_), _json0_); \
		_json1 =  metac_type_and_ptr2json_string(&METAC_TYPE_NAME(_type_), &expected_data, \
				(_expected_data_len_ == -1)?sizeof(expected_data):(_expected_data_len_)); \
		json1 = _json1?strdupa(_json1):NULL; free(_json1); _json1= NULL; \
		_cdata[1] = metac_json2object(&METAC_TYPE_NAME(_type_), _json1_); \
		fail_unless(_cdata[0] != NULL, "Des11n: metac_json2object returned NULL for %s", _json0_); \
		fail_unless(_cdata[0]->ptr != NULL, "Des11n: cdata0->ptr is NULL"); \
		fail_unless(_cdata[1] != NULL, "Des11n: metac_json2object returned NULL for %s", _json1_); \
		fail_unless(_cdata[1]->ptr != NULL, "Des11n: cdata1->ptr is NULL"); \
		fail_unless(json1 != NULL, "S11n: Got Null instead of json1"); \
		fail_unless(strcmp(json1, _json1_) == 0, "S11n: Expected %s, and got %s", _json1_, json1); \
		for (i = 0; i < 2; i++) { \
			cdata = _cdata[i]; \
			json = _json[i];

#define JSON_POSITIVE_BEGIN(_static_, _type_, _json0_, _json1_, _expected_data_...) \
		JSON_POSITIVE_BEGIN_WITH_LEN(_static_, _type_, _json0_, _json1_, -1, _expected_data_)

#define JSON_POSITIVE_END \
			fail_unless(metac_object_put(cdata) != 0, "Couldn't delete created object cdata%d", i); \
		}\
		mark_point(); \
	}while(0)

/* special cases for positive UTs */
#define BASIC_TYPE_JSON_POSITIVE(_type_, _json0_, _json1_, _expected_data_...) \
	JSON_POSITIVE_BEGIN(static, _type_, _json0_, _json1_, _expected_data_) \
		fail_unless((*((_type_*)cdata->ptr)) == (_expected_data_), "Des11n: unexpected data for %s: %Lf, expected %s", json, (long double)(*((_type_*)cdata->ptr)), #_expected_data_); \
	JSON_POSITIVE_END

#define PCHAR_TYPE_JSON_POSITIVE(_type_, _json0_, _json1_, _expected_data_...) \
	JSON_POSITIVE_BEGIN(static,_type_, _json0_, _json1_, _expected_data_) \
		fail_unless(strcmp((*((_type_*)cdata->ptr)), (expected_data)) == 0, "Des11n: unexpected data for %s", json); \
	JSON_POSITIVE_END

#define STRUCT_TYPE_JSON_POSITIVE(_type_, _json0_, _json1_, _expected_data_...) \
	JSON_POSITIVE_BEGIN(static, _type_, _json0_, _json1_, _expected_data_) \
		if (memcmp((_type_*)cdata->ptr, &expected_data, sizeof(expected_data))!=0) { \
			DUMP_MEM("data         : ", cdata->ptr, sizeof(expected_data)); \
			DUMP_MEM("expected data: ", &expected_data, sizeof(expected_data)); \
			fail_unless(0, "Des11n: unexpected data for %s, expected %s", json, #_expected_data_); \
		} \
	JSON_POSITIVE_END

#define ARRAY_TYPE_JSON_POSITIVE STRUCT_TYPE_JSON_POSITIVE

/*---------------*/
#define JSON_S11N_NEGATIVE_WITH_LEN(_type_, _data_len_, _data_...) \
	do { \
		static _type_ data = _data_; \
		char * _json1 =  metac_type_and_ptr2json_string(&METAC_TYPE_NAME(_type_), &data, _data_len_); \
		char * json1 = _json1?strdupa(_json1):NULL; free(_json1); _json1= NULL; \
		fail_unless(json1 == NULL, "metac_type_and_ptr2json_string must return NULL, but returned %s", json1); \
		mark_point(); \
	}while(0)

#define JSON_S11N_NEGATIVE(_type_, _data_...) JSON_S11N_NEGATIVE(_type_, sizeof(_data_), _data_)

#define JSON_DES11N_NEGATIVE(_type_, _json_str_) \
	do { \
		fail_unless((metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) == NULL, "metac_json2object must return NULL for %s", _json_str_); \
		mark_point(); \
	}while(0)

/*****************************************************************************/
METAC_TYPE_GENERATE(char);

START_TEST(ut_char){
	char buf[30];
	BASIC_TYPE_JSON_POSITIVE(char, "\"c\"", "\"c\"", 'c');
	BASIC_TYPE_JSON_POSITIVE(char, "\"0\"", "\"0\"", '0');
	BASIC_TYPE_JSON_POSITIVE(char, "\"A\"", "\"A\"", 'A');
	BASIC_TYPE_JSON_POSITIVE(char, "\"z\"", "\"z\"", 'z');
	BASIC_TYPE_JSON_POSITIVE(char, "\"&\"", "\"&\"", '&');
	BASIC_TYPE_JSON_POSITIVE(char, "7", "7", 0x7);
	snprintf(buf, sizeof(buf), "%d", INT8_MIN);
	BASIC_TYPE_JSON_POSITIVE(char, buf, buf, INT8_MIN);
	snprintf(buf, sizeof(buf), "%d", INT8_MAX);
	BASIC_TYPE_JSON_POSITIVE(char, buf, buf, INT8_MAX);

	JSON_DES11N_NEGATIVE(char, "\"cc\"");
	JSON_DES11N_NEGATIVE(char, "-7.9");
	JSON_DES11N_NEGATIVE(char, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(char, "{\"xx\": \"xy\"}");
	JSON_DES11N_NEGATIVE(char, "\"cc\"");
	JSON_DES11N_NEGATIVE(char, "-7.9");
	JSON_DES11N_NEGATIVE(char, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(char, "{\"xx\": \"xy\"}");

	JSON_S11N_NEGATIVE_WITH_LEN(char, 2, 'c');
	JSON_S11N_NEGATIVE_WITH_LEN(char, 0, 'c');
}END_TEST

/*****************************************************************************/
typedef unsigned char uchar_t;
METAC_TYPE_GENERATE(uchar_t);

START_TEST(ut_uchar_t){
	char buf[30];
	BASIC_TYPE_JSON_POSITIVE(uchar_t, "\"c\"", "\"c\"", 'c');
	BASIC_TYPE_JSON_POSITIVE(uchar_t, "\"0\"", "\"0\"", '0');
	BASIC_TYPE_JSON_POSITIVE(uchar_t, "\"A\"", "\"A\"", 'A');
	BASIC_TYPE_JSON_POSITIVE(uchar_t, "\"z\"", "\"z\"", 'z');
	BASIC_TYPE_JSON_POSITIVE(uchar_t, "\"&\"", "\"&\"", '&');
	BASIC_TYPE_JSON_POSITIVE(uchar_t, "7", "7", 0x7);
	BASIC_TYPE_JSON_POSITIVE(uchar_t, "0", "0", 0);
	snprintf(buf, sizeof(buf), "%ld", (long)UINT8_MAX);
	BASIC_TYPE_JSON_POSITIVE(uchar_t, buf, buf, UINT8_MAX);

	JSON_DES11N_NEGATIVE(uchar_t, "\"cc\"");
	JSON_DES11N_NEGATIVE(uchar_t, "-7.9");
	JSON_DES11N_NEGATIVE(uchar_t, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(uchar_t, "{\"xx\": \"xy\"}");

	JSON_S11N_NEGATIVE_WITH_LEN(uchar_t, 2, 'c');
	JSON_S11N_NEGATIVE_WITH_LEN(uchar_t, 0, 'c');
}END_TEST

/*****************************************************************************/
METAC_TYPE_GENERATE(short);
START_TEST(ut_short){
	char buf1[30], buf2[30];
	BASIC_TYPE_JSON_POSITIVE(short, "0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(short, "-0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(short, "1234", "\"1234\"", 1234);
	BASIC_TYPE_JSON_POSITIVE(short, "-10", "\"-10\"", -10);

	snprintf(buf1, sizeof(buf1), "%ld", (long)INT16_MAX);
	snprintf(buf2, sizeof(buf2), "\"%ld\"", (long)INT16_MAX);
	BASIC_TYPE_JSON_POSITIVE(short, buf1, buf2, INT16_MAX);

	snprintf(buf1, sizeof(buf1), "%ld", (long)INT16_MIN);
	snprintf(buf2, sizeof(buf2), "\"%ld\"", (long)INT16_MIN);
	BASIC_TYPE_JSON_POSITIVE(short, buf1, buf2, INT16_MIN);

	BASIC_TYPE_JSON_POSITIVE(short, "\"-7\"", "\"-7\"", -7);
	BASIC_TYPE_JSON_POSITIVE(short, "\"-0\"", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(short, "\"-07\"", "\"-7\"", -07);
	BASIC_TYPE_JSON_POSITIVE(short, "\"07\"", "\"7\"", 07);
	BASIC_TYPE_JSON_POSITIVE(short, "\"076234\"", "\"31900\"", 076234);
	BASIC_TYPE_JSON_POSITIVE(short, "\"-076234\"", "\"-31900\"", -076234);
	BASIC_TYPE_JSON_POSITIVE(short, "\"-0xa\"", "\"-10\"", -0xa);
	BASIC_TYPE_JSON_POSITIVE(short, "\"0x7fff\"", "\"32767\"", 0x7fff);
	BASIC_TYPE_JSON_POSITIVE(short, "\"-0x8000\"", "\"-32768\"", -0x8000);

	JSON_DES11N_NEGATIVE(short, "\"-0a\"");
	JSON_DES11N_NEGATIVE(short, "\"-01a\"");
	JSON_DES11N_NEGATIVE(short, "\"-0x\"");
	JSON_DES11N_NEGATIVE(short, "\"-x\"");
	JSON_DES11N_NEGATIVE(short, "\"c\"");
	JSON_DES11N_NEGATIVE(short, "\"cc\"");
	JSON_DES11N_NEGATIVE(short, "-7.9");
	JSON_DES11N_NEGATIVE(short, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(short, "{\"xx\": \"xy\"}");

	JSON_S11N_NEGATIVE_WITH_LEN(short, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(short, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(short, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(short, 4, 7);
}END_TEST

/*****************************************************************************/
typedef unsigned short ushort_t;
METAC_TYPE_GENERATE(ushort_t);

START_TEST(ut_ushort_t){
	char buf1[30], buf2[30];
	BASIC_TYPE_JSON_POSITIVE(ushort_t, "0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(ushort_t, "-0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(ushort_t, "1234", "\"1234\"", 1234);
	BASIC_TYPE_JSON_POSITIVE(ushort_t, "\"0\"", "\"0\"", 0);

	snprintf(buf1, sizeof(buf1), "%ld", (long)UINT16_MAX);
	snprintf(buf2, sizeof(buf2), "\"%ld\"", (long)UINT16_MAX);
	BASIC_TYPE_JSON_POSITIVE(ushort_t, buf1, buf2, UINT16_MAX);

	BASIC_TYPE_JSON_POSITIVE(ushort_t, "\"07\"", "\"7\"", 07);
	BASIC_TYPE_JSON_POSITIVE(ushort_t, "\"076234\"", "\"31900\"", 076234);
	BASIC_TYPE_JSON_POSITIVE(ushort_t, "\"0xffff\"", "\"65535\"", 0xffff);

	JSON_DES11N_NEGATIVE(ushort_t, "\"-7\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"-07\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"-076234\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"-0xa\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"-0x8000\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"-0a\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"-01a\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"-0x\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"-x\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"c\"");
	JSON_DES11N_NEGATIVE(ushort_t, "\"cc\"");
	JSON_DES11N_NEGATIVE(ushort_t, "-7.9");
	JSON_DES11N_NEGATIVE(ushort_t, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(ushort_t, "{\"xx\": \"xy\"}");

	JSON_S11N_NEGATIVE_WITH_LEN(ushort_t, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ushort_t, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ushort_t, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ushort_t, 4, 7);
}END_TEST

/*****************************************************************************/
METAC_TYPE_GENERATE(int);

START_TEST(ut_int){
	char buf1[30], buf2[30];
	BASIC_TYPE_JSON_POSITIVE(int, "0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(int, "-0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(int, "1234", "\"1234\"", 1234);
	BASIC_TYPE_JSON_POSITIVE(int, "-10", "\"-10\"", -10);

	snprintf(buf1, sizeof(buf1), "%d", INT32_MIN);
	snprintf(buf2, sizeof(buf2), "\"%d\"", INT32_MIN);
	BASIC_TYPE_JSON_POSITIVE(int, buf1, buf2, INT32_MIN);

	snprintf(buf1, sizeof(buf1), "%d", INT32_MAX);
	snprintf(buf2, sizeof(buf2), "\"%d\"", INT32_MAX);
	BASIC_TYPE_JSON_POSITIVE(int, buf1, buf2, INT32_MAX);

	BASIC_TYPE_JSON_POSITIVE(int, "\"-7\"", "\"-7\"", -7);
	BASIC_TYPE_JSON_POSITIVE(int, "\"-0\"", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(int, "\"-07\"", "\"-7\"", -07);
	BASIC_TYPE_JSON_POSITIVE(int, "\"07\"", "\"7\"", 07);
	BASIC_TYPE_JSON_POSITIVE(int, "\"076234\"", "\"31900\"", 076234);
	BASIC_TYPE_JSON_POSITIVE(int, "\"-076234\"", "\"-31900\"", -076234);
	BASIC_TYPE_JSON_POSITIVE(int, "\"-0xa\"", "\"-10\"", -0xa);
	BASIC_TYPE_JSON_POSITIVE(int, "\"0x7fffffff\"", "\"2147483647\"", 0x7fffffff);
	BASIC_TYPE_JSON_POSITIVE(int, "\"-0x80000000\"", "\"-2147483648\"", -0x80000000);
	JSON_DES11N_NEGATIVE(int, "\"-0a\"");
	JSON_DES11N_NEGATIVE(int, "\"-01a\"");
	JSON_DES11N_NEGATIVE(int, "\"-0x\"");
	JSON_DES11N_NEGATIVE(int, "\"-x\"");
	JSON_DES11N_NEGATIVE(int, "\"c\"");
	JSON_DES11N_NEGATIVE(int, "\"cc\"");
	JSON_DES11N_NEGATIVE(int, "-7.9");
	JSON_DES11N_NEGATIVE(int, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(int, "{\"xx\": \"xy\"}");
	JSON_S11N_NEGATIVE_WITH_LEN(int, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(int, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(int, 2, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(int, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(int, 5, 7);
}END_TEST

/*****************************************************************************/
typedef unsigned int uint_t;
METAC_TYPE_GENERATE(uint_t);

START_TEST(ut_uint_t){
	char buf1[30], buf2[30];
	BASIC_TYPE_JSON_POSITIVE(uint_t, "0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(uint_t, "-0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(uint_t, "1234", "\"1234\"", 1234);
	BASIC_TYPE_JSON_POSITIVE(uint_t, "\"0\"", "\"0\"", 0);
#if JSON_C_MAJOR_VERSION > 0 || JSON_C_MINOR_VERSION >= 10
	snprintf(buf1, sizeof(buf1), "%u", UINT32_MAX);
	snprintf(buf2, sizeof(buf2), "\"%u\"", UINT32_MAX);
	BASIC_TYPE_JSON_POSITIVE(uint_t, buf, UINT32_MAX);
#endif
	BASIC_TYPE_JSON_POSITIVE(uint_t, "\"07\"", "\"7\"", 07);
	BASIC_TYPE_JSON_POSITIVE(uint_t, "\"076234\"", "\"31900\"", 076234);
	BASIC_TYPE_JSON_POSITIVE(uint_t, "\"0xffffffff\"", "\"4294967295\"", 0xffffffff);
	JSON_DES11N_NEGATIVE(uint_t, "-10");
	JSON_DES11N_NEGATIVE(uint_t, "\"-7\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"-07\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"-076234\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"-0xa\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"-0x80000000\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"-0a\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"-01a\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"-0x\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"-x\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"c\"");
	JSON_DES11N_NEGATIVE(uint_t, "\"cc\"");
	JSON_DES11N_NEGATIVE(uint_t, "-7.9");
	JSON_DES11N_NEGATIVE(uint_t, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(uint_t, "{\"xx\": \"xy\"}");
	JSON_S11N_NEGATIVE_WITH_LEN(uint_t, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(uint_t, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(uint_t, 2, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(uint_t, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(uint_t, 5, 7);
}END_TEST

/*****************************************************************************/
METAC_TYPE_GENERATE(long);

START_TEST(ut_long){
	char buf1[30], buf2[30];
	BASIC_TYPE_JSON_POSITIVE(long, "0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(long, "-0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(long, "1234", "\"1234\"", 1234);
	BASIC_TYPE_JSON_POSITIVE(long, "-10", "\"-10\"", -10);

#if JSON_C_MAJOR_VERSION > 0 || JSON_C_MINOR_VERSION >= 10
	snprintf(buf1, sizeof(buf1), "%ld", INT64_MIN);
	snprintf(buf2, sizeof(buf2), "%ld", INT64_MIN);
	BASIC_TYPE_JSON_POSITIVE(long, buf, INT64_MIN);
	snprintf(buf1, sizeof(buf1), "%ld", (long)INT64_MAX);
	snprintf(buf2, sizeof(buf2), "\"%ld\"", (long)INT64_MAX);
	BASIC_TYPE_JSON_POSITIVE(long, buf, INT64_MAX);
#endif

	BASIC_TYPE_JSON_POSITIVE(long, "\"-7\"", "\"-7\"", -7);
	BASIC_TYPE_JSON_POSITIVE(long, "\"-0\"", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(long, "\"-07\"", "\"-7\"", -07);
	BASIC_TYPE_JSON_POSITIVE(long, "\"07\"", "\"7\"", 07);
	BASIC_TYPE_JSON_POSITIVE(long, "\"076234\"", "\"31900\"", 076234);
	BASIC_TYPE_JSON_POSITIVE(long, "\"-076234\"", "\"-31900\"", -076234);
	BASIC_TYPE_JSON_POSITIVE(long, "\"-0xa\"", "\"-10\"", -0xa);
	BASIC_TYPE_JSON_POSITIVE(long, "\"0x7fffffff\"", "\"2147483647\"", 0x7fffffff);
	BASIC_TYPE_JSON_POSITIVE(long, "\"-0x80000000\"", "\"-2147483648\"", -0x80000000L);
	BASIC_TYPE_JSON_POSITIVE(long, "\"0x7fffffffffffffff\"", "\"9223372036854775807\"", 0x7fffffffffffffffL);
	BASIC_TYPE_JSON_POSITIVE(long, "\"-0x8000000000000000\"", "\"-9223372036854775808\"", -0x8000000000000000L);
	JSON_DES11N_NEGATIVE(long, "\"-0a\"");
	JSON_DES11N_NEGATIVE(long, "\"-01a\"");
	JSON_DES11N_NEGATIVE(long, "\"-0x\"");
	JSON_DES11N_NEGATIVE(long, "\"-x\"");
	JSON_DES11N_NEGATIVE(long, "\"c\"");
	JSON_DES11N_NEGATIVE(long, "\"cc\"");
	JSON_DES11N_NEGATIVE(long, "-7.9");
	JSON_DES11N_NEGATIVE(long, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(long, "{\"xx\": \"xy\"}");
	JSON_S11N_NEGATIVE_WITH_LEN(long, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 2, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 4, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 5, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 6, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 7, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 9, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(long, 10, 7);
}END_TEST

/*****************************************************************************/
typedef unsigned long ulong_t;
METAC_TYPE_GENERATE(ulong_t);

START_TEST(ut_ulong_t){
	BASIC_TYPE_JSON_POSITIVE(ulong_t, "0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(ulong_t, "-0", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(ulong_t, "1234", "\"1234\"", 1234);
	BASIC_TYPE_JSON_POSITIVE(ulong_t, "\"0\"", "\"0\"", 0);
	BASIC_TYPE_JSON_POSITIVE(ulong_t, "\"07\"", "\"7\"", 07);
	BASIC_TYPE_JSON_POSITIVE(ulong_t, "\"076234\"", "\"31900\"", 076234);
	BASIC_TYPE_JSON_POSITIVE(ulong_t, "\"0xffffffff\"", "\"4294967295\"", 0xffffffff);
	BASIC_TYPE_JSON_POSITIVE(ulong_t, "\"0xffffffffffffffff\"", "\"18446744073709551615\"", 0xffffffffffffffffL);
	JSON_DES11N_NEGATIVE(ulong_t, "-10");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-7\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-07\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-076234\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-0xa\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-0x80000000\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-0a\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-01a\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-0x\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"-x\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"c\"");
	JSON_DES11N_NEGATIVE(ulong_t, "\"cc\"");
	JSON_DES11N_NEGATIVE(ulong_t, "-7.9");
	JSON_DES11N_NEGATIVE(ulong_t, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(ulong_t, "{\"xx\": \"xy\"}");
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 2, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 4, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 5, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 6, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 7, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 9, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ulong_t, 10, 7);
}END_TEST

/*****************************************************************************/
METAC_TYPE_GENERATE(float);

START_TEST(ut_float){
	char buf1[30];
	BASIC_TYPE_JSON_POSITIVE(float, "0", "\"0.000000\"", 0);
	BASIC_TYPE_JSON_POSITIVE(float, "-0", "\"0.000000\"", 0);
	BASIC_TYPE_JSON_POSITIVE(float, "1234", "\"1234.000000\"", 1234);
	BASIC_TYPE_JSON_POSITIVE(float, "0.7", "\"0.700000\"", 0.7f);
	BASIC_TYPE_JSON_POSITIVE(float, "-0.7", "\"-0.700000\"", -0.7f);
	BASIC_TYPE_JSON_POSITIVE(float, "\"0.7\"", "\"0.700000\"", 0.7f);
	BASIC_TYPE_JSON_POSITIVE(float, "\"0.76234\"", "\"0.762340\"", 0.76234f);

	snprintf(buf1, sizeof(buf1), "\"%f\"", -1024.934f);
	BASIC_TYPE_JSON_POSITIVE(float, "\"-1024.934\"", buf1, -1024.934f);

	BASIC_TYPE_JSON_POSITIVE(float, "\"-07\"", "\"-7.000000\"", -07.0f);
	BASIC_TYPE_JSON_POSITIVE(float, "\"-076234\"", "\"-76234.000000\"", -076234.0f);
	BASIC_TYPE_JSON_POSITIVE(float, "\"-0xa.0\"", "\"-10.000000\"", -10.0f);
	BASIC_TYPE_JSON_POSITIVE(float, "\"-0x80000000\"", "\"-2147483648.000000\"", -2147483648.0f);
	JSON_DES11N_NEGATIVE(float, "\"-0a\"");
	JSON_DES11N_NEGATIVE(float, "\"-01a\"");
	JSON_DES11N_NEGATIVE(float, "\"-0x\"");
	JSON_DES11N_NEGATIVE(float, "\"-x\"");
	JSON_DES11N_NEGATIVE(float, "\"c\"");
	JSON_DES11N_NEGATIVE(float, "\"cc\"");
	JSON_DES11N_NEGATIVE(float, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(float, "{\"xx\": \"xy\"}");
	JSON_S11N_NEGATIVE_WITH_LEN(float, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(float, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(float, 2, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(float, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(float, 5, 7);
}END_TEST

/*****************************************************************************/
METAC_TYPE_GENERATE(double);

START_TEST(ut_double){
	BASIC_TYPE_JSON_POSITIVE(double, "0", "\"0.000000\"",  0);
	BASIC_TYPE_JSON_POSITIVE(double, "-0", "\"0.000000\"", 0);
	BASIC_TYPE_JSON_POSITIVE(double, "1234", "\"1234.000000\"", 1234);
	BASIC_TYPE_JSON_POSITIVE(double, "0.7", "\"0.700000\"", 0.7);
	BASIC_TYPE_JSON_POSITIVE(double, "-0.7", "\"-0.700000\"", -0.7);
	BASIC_TYPE_JSON_POSITIVE(double, "\"0.7\"", "\"0.700000\"", 0.7);
	BASIC_TYPE_JSON_POSITIVE(double, "\"0.76234\"", "\"0.762340\"", 0.76234);
	BASIC_TYPE_JSON_POSITIVE(double, "\"-1024.934\"", "\"-1024.934000\"", -1024.934);
	BASIC_TYPE_JSON_POSITIVE(double, "\"-07\"", "\"-7.000000\"", -07.0);
	BASIC_TYPE_JSON_POSITIVE(double, "\"-076234\"", "\"-76234.000000\"", -076234.0);
	BASIC_TYPE_JSON_POSITIVE(double, "\"-0xa.0\"", "\"-10.000000\"", -10.0);
	BASIC_TYPE_JSON_POSITIVE(double, "\"-0x80000000\"", "\"-2147483648.000000\"", -2147483648.0);
	JSON_DES11N_NEGATIVE(double, "\"-0a\"");
	JSON_DES11N_NEGATIVE(double, "\"-01a\"");
	JSON_DES11N_NEGATIVE(double, "\"-0x\"");
	JSON_DES11N_NEGATIVE(double, "\"-x\"");
	JSON_DES11N_NEGATIVE(double, "\"c\"");
	JSON_DES11N_NEGATIVE(double, "\"cc\"");
	JSON_DES11N_NEGATIVE(double, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(double, "{\"xx\": \"xy\"}");
	JSON_S11N_NEGATIVE_WITH_LEN(double, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 2, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 4, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 5, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 6, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 7, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 9, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(double, 10, 7);
}END_TEST

/*****************************************************************************/
typedef long double ldouble_t;
METAC_TYPE_GENERATE(ldouble_t);

START_TEST(ut_ldouble_t){
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "0", "\"0.000000\"", 0);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "-0", "\"0.000000\"", 0);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "1234", "\"1234.000000\"", 1234);
	/* For the next 2 tests:
	 * 0.7L/-0.7L will not work because libjson returns double and we convert it to long double
	 * and it will not be equal to 0.7L (some difference in lower bits)
	 */
//	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "0.7", "\"0.700000\"", 0.7);
//	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "-0.7", "\"-0.700000\"", -0.7);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "\"0.7\"", "\"0.700000\"",0.7L);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "\"0.76234\"", "\"0.762340\"", 0.76234L);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "\"-1024.934\"", "\"-1024.934000\"", -1024.934L);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "\"-07\"", "\"-7.000000\"", -07.0);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "\"-076234\"", "\"-76234.000000\"", -076234.0);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "\"-0xa.0\"", "\"-10.000000\"", -10.0L);
	BASIC_TYPE_JSON_POSITIVE(ldouble_t, "\"-0x80000000\"", "\"-2147483648.000000\"", -2147483648.0);
	JSON_DES11N_NEGATIVE(ldouble_t, "\"-0a\"");
	JSON_DES11N_NEGATIVE(ldouble_t, "\"-01a\"");
	JSON_DES11N_NEGATIVE(ldouble_t, "\"-0x\"");
	JSON_DES11N_NEGATIVE(ldouble_t, "\"-x\"");
	JSON_DES11N_NEGATIVE(ldouble_t, "\"c\"");
	JSON_DES11N_NEGATIVE(ldouble_t, "\"cc\"");
	JSON_DES11N_NEGATIVE(ldouble_t, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(ldouble_t, "{\"xx\": \"xy\"}");
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 0, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 1, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 2, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 3, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 4, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 5, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 6, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 7, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 8, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 9, 7);
	JSON_S11N_NEGATIVE_WITH_LEN(ldouble_t, 11, 7);
}END_TEST

/*****************************************************************************/
typedef char* pchar_t;
METAC_TYPE_GENERATE(pchar_t);

START_TEST(ut_pchar_t){
	PCHAR_TYPE_JSON_POSITIVE(pchar_t, "\"string1234\"", "\"string1234\"", "string1234");
	PCHAR_TYPE_JSON_POSITIVE(pchar_t,
			"[\"s\", \"t\", \"r\", \"i\", \"n\", \"g\", \"1\", \"2\", \"3\" ,\"4\", 0]",
			"\"string1234\"",
			"string1234");
	JSON_S11N_NEGATIVE_WITH_LEN(pchar_t, 3, "\"string1234\"");
}END_TEST

/*****************************************************************************/
typedef enum _enum_{
	_eZero = 0,
	_eOne,
	_eTen = 10,
	_eEleven,
	_eTwelve,
}enum_t;
METAC_TYPE_GENERATE(enum_t);

START_TEST(ut_enum_t){
	BASIC_TYPE_JSON_POSITIVE(enum_t, "\"_eZero\"", "\"_eZero\"", _eZero);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "\"_eOne\"", "\"_eOne\"", _eOne);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "\"_eTen\"", "\"_eTen\"", _eTen);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "\"_eEleven\"", "\"_eEleven\"", _eEleven);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "\"_eTwelve\"", "\"_eTwelve\"", _eTwelve);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "0", "\"_eZero\"", _eZero);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "1", "\"_eOne\"",_eOne);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "10", "\"_eTen\"", _eTen);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "11", "\"_eEleven\"", _eEleven);
	BASIC_TYPE_JSON_POSITIVE(enum_t, "12", "\"_eTwelve\"", _eTwelve);
	JSON_DES11N_NEGATIVE(enum_t, "13");
	JSON_DES11N_NEGATIVE(enum_t, "\"_eZerox\"");
	JSON_DES11N_NEGATIVE(enum_t, "\"x_eZero\"");
	JSON_DES11N_NEGATIVE(enum_t, "\"c\"");
	JSON_DES11N_NEGATIVE(enum_t, "\"cc\"");
	JSON_DES11N_NEGATIVE(enum_t, "-7.9");
	JSON_DES11N_NEGATIVE(enum_t, "[\"xx\", \"xy\"]");
	JSON_DES11N_NEGATIVE(enum_t, "{\"xx\": \"xy\"}");
	JSON_S11N_NEGATIVE_WITH_LEN(enum_t, 0, _eZero);
	JSON_S11N_NEGATIVE_WITH_LEN(enum_t, 2, _eZero);
	JSON_S11N_NEGATIVE_WITH_LEN(enum_t, 3, _eZero);
}END_TEST

/*****************************************************************************/
typedef struct struct1 {
	int x;
	unsigned int y;
}struct1_t;
METAC_TYPE_GENERATE(struct1_t);

START_TEST(ut_struct1_t){
	STRUCT_TYPE_JSON_POSITIVE(struct1_t, "{}", "{ \"x\": \"0\", \"y\": \"0\" }", {});
	STRUCT_TYPE_JSON_POSITIVE(struct1_t, "{\"x\": -1}", "{ \"x\": \"-1\", \"y\": \"0\" }", {.x = -1});
	STRUCT_TYPE_JSON_POSITIVE(struct1_t, "{\"y\": 1}", "{ \"x\": \"0\", \"y\": \"1\" }", {.y = 1});
	STRUCT_TYPE_JSON_POSITIVE(struct1_t, "{\"x\": -1, \"y\": 1}", "{ \"x\": \"-1\", \"y\": \"1\" }", {.x = -1, .y = 1});
	JSON_DES11N_NEGATIVE(struct1_t, "{\"y\": -1}");
}END_TEST

/*****************************************************************************/
typedef struct struct2 {
	struct1_t * str1;
}struct2_t;
METAC_TYPE_GENERATE(struct2_t);

START_TEST(ut_struct2_t){
	struct1_t struct1 = {.x = 1, .y = 8};
	JSON_POSITIVE_BEGIN(, struct2_t,
			"{\"str1\":{\"x\": 1, \"y\": 8}}",
			"{ \"str1\": { \"x\": \"1\", \"y\": \"8\" } }",
			{.str1 = &struct1} ) {
		struct2_t *real_data = (struct2_t*)cdata->ptr;
		fail_unless(
				expected_data.str1->x == real_data->str1->x &&
				expected_data.str1->y == real_data->str1->y, "unexpected data");
	}JSON_POSITIVE_END;
	JSON_DES11N_NEGATIVE(struct2_t, "{\"str1\":\"string\"}");
}END_TEST

/*****************************************************************************/
typedef struct bit_fields1 {
	unsigned int field_15b: 15;
	unsigned int field_17b: 17;
}bit_fields1_t;
METAC_TYPE_GENERATE(bit_fields1_t);

START_TEST(ut_bit_fields1_t){
	STRUCT_TYPE_JSON_POSITIVE(bit_fields1_t,
			"{}",
			"{ \"field_15b\": \"0\", \"field_17b\": \"0\" }",
			{});
	STRUCT_TYPE_JSON_POSITIVE(bit_fields1_t,
			"{\"field_15b\": 6}",
			"{ \"field_15b\": \"6\", \"field_17b\": \"0\" }",
			{.field_15b = 6});
	STRUCT_TYPE_JSON_POSITIVE(bit_fields1_t,
			"{\"field_17b\": 6000}",
			"{ \"field_15b\": \"0\", \"field_17b\": \"6000\" }",
			{.field_17b = 6000});
	STRUCT_TYPE_JSON_POSITIVE(bit_fields1_t, "{\"field_15b\": 6, \"field_17b\": 100}",
			"{ \"field_15b\": \"6\", \"field_17b\": \"100\" }",
			{.field_15b = 6, .field_17b = 100});
	STRUCT_TYPE_JSON_POSITIVE(bit_fields1_t,
			"{\"field_15b\": 16384, \"field_17b\": 65536}",
			"{ \"field_15b\": \"16384\", \"field_17b\": \"65536\" }",
			{.field_15b = 16384, .field_17b = 65536});
	JSON_DES11N_NEGATIVE(bit_fields1_t, "{\"field_15b\": -6}");
}END_TEST

/*****************************************************************************/
typedef struct bit_fields2 {
	unsigned int field_9b: 9;
	unsigned int field_12b: 12;
	long field_36b: 36;
	long field_63b: 63;
}bit_fields2_t;
METAC_TYPE_GENERATE(bit_fields2_t);

START_TEST(ut_bit_fields2_t){
	STRUCT_TYPE_JSON_POSITIVE(bit_fields2_t,
			"{\"field_9b\": 6, \"field_12b\": 100, \"field_36b\": -1, \"field_63b\": 1000}",
			"{ \"field_9b\": \"6\", \"field_12b\": \"100\", \"field_36b\": \"-1\", \"field_63b\": \"1000\" }",
			{.field_9b = 6, .field_12b = 100, .field_36b = -1, .field_63b = 1000});

}END_TEST

/*****************************************************************************/
typedef char char_array5_t[5];
METAC_TYPE_GENERATE(char_array5_t);

START_TEST(ut_char_array5_t){
	ARRAY_TYPE_JSON_POSITIVE(char_array5_t,
			"[\"a\",\"b\",\"c\",\"d\",\"e\"]",
			"[ \"a\", \"b\", \"c\", \"d\", \"e\" ]",
			{'a', 'b', 'c', 'd', 'e'});
	JSON_DES11N_NEGATIVE(char_array5_t, "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\"]");
}END_TEST

/*****************************************************************************/
typedef char* pchar_array5_t[5];
METAC_TYPE_GENERATE(pchar_array5_t);

START_TEST(ut_pchar_array5_t){
	JSON_POSITIVE_BEGIN(, pchar_array5_t,
			"[\"a\",\"b\",\"c\",\"d\",\"e\"]",
			"[ \"a\", \"b\", \"c\", \"d\", \"e\" ]",
			{"a", "b", "c", "d", "e"}) {
		int i;
		for (i = 0; i < sizeof(expected_data)/sizeof(expected_data[0]); ++i) {
			char * element = (*((pchar_array5_t*)cdata->ptr))[i];
			fail_unless(strcmp(element, expected_data[i]) == 0, "got incorrect value %s, expected %s", element, expected_data[i]);
		}
	}JSON_POSITIVE_END;
}END_TEST


/*****************************************************************************/
typedef struct struct3 {
	int x;
	char flex_arr3[];
}struct3_t;
METAC_TYPE_GENERATE(struct3_t);

START_TEST(ut_struct3_t){
	JSON_POSITIVE_BEGIN_WITH_LEN(static, struct3_t,
			"{\"flex_arr3\": [\"a\", \"b\", \"c\", \"d\", \"e\"]}",
			"{ \"x\": \"0\", \"flex_arr3\": [ \"a\", \"b\", \"c\", \"d\", \"e\" ] }",
			sizeof(struct3_t) + 6,
			{.flex_arr3 = {'a', 'b', 'c', 'd', 'e', 0}}) {
		int i;
		struct3_t * _struct3 = (struct3_t*)cdata->ptr;
		for (i = 0; i < 6; ++i) {
			fail_unless(_struct3->flex_arr3[i] == expected_data.flex_arr3[i], "got incorrect value %c, expected %c",
					_struct3->flex_arr3[i], expected_data.flex_arr3[i]);
		}
	}JSON_POSITIVE_END;
}END_TEST

/*****************************************************************************/
typedef struct struct4 {
	int flex_arr4_len;
	char flex_arr4[];
}struct4_t;
METAC_TYPE_GENERATE(struct4_t);

START_TEST(ut_struct4_t){
	JSON_POSITIVE_BEGIN_WITH_LEN(static, struct4_t,
			"{\"flex_arr4_len\": 5, \"flex_arr4\": [\"a\", \"b\", \"c\", \"d\", \"e\"]}",
			"{ \"flex_arr4_len\": \"5\", \"flex_arr4\": [ \"a\", \"b\", \"c\", \"d\", \"e\" ] }",
			sizeof(struct4_t) + 5,
			{.flex_arr4_len = 5, .flex_arr4 = {'a', 'b', 'c', 'd', 'e'}}) {
		int i;
		struct4_t * _struct4 = (struct4_t*)cdata->ptr;
		fail_unless(_struct4->flex_arr4_len == expected_data.flex_arr4_len, "Len must be equal %d, %d",
				(int)_struct4->flex_arr4_len, (int)expected_data.flex_arr4_len);
		for (i = 0; i < _struct4->flex_arr4_len; ++i) {
			fail_unless(_struct4->flex_arr4[i] == expected_data.flex_arr4[i], "got incorrect value %c, expected %c",
					_struct4->flex_arr4[i], expected_data.flex_arr4[i]);
		}
	}JSON_POSITIVE_END;
	JSON_POSITIVE_BEGIN_WITH_LEN(static, struct4_t,
			"{\"flex_arr4\": [\"a\", \"b\", \"c\", \"d\", \"e\"]}",
			"{ \"flex_arr4_len\": \"5\", \"flex_arr4\": [ \"a\", \"b\", \"c\", \"d\", \"e\" ] }",
			sizeof(struct4_t) + 5,
			{.flex_arr4_len = 5, .flex_arr4 = {'a', 'b', 'c', 'd', 'e'}}) {
		int i;
		struct4_t * _struct4 = (struct4_t*)cdata->ptr;
		fail_unless(_struct4->flex_arr4_len == expected_data.flex_arr4_len, "Len must be equal %d, %d",
				(int)_struct4->flex_arr4_len, (int)expected_data.flex_arr4_len);
		for (i = 0; i < _struct4->flex_arr4_len; ++i) {
			fail_unless(_struct4->flex_arr4[i] == expected_data.flex_arr4[i], "got incorrect value %c, expected %c",
					_struct4->flex_arr4[i], expected_data.flex_arr4[i]);
		}
	}JSON_POSITIVE_END;
}END_TEST

/*****************************************************************************/
typedef struct struct5 {
	int u_descriminator;
	union {
		struct {
			uint8_t byte[4];
		}b;
		struct {
			uint16_t word[2];
		}w;
		struct {
			uint32_t dword[1];
		}dw;
	}u;
}struct5_t;
METAC_TYPE_GENERATE(struct5_t);

START_TEST(ut_struct5_t){
	STRUCT_TYPE_JSON_POSITIVE(struct5_t,
			"{\"u_descriminator\": 0, \"u\": {\"b\": {\"byte\": [1, 2, 3, 4]}}}",
			"{ \"u_descriminator\": \"0\", \"u\": { \"b\": { \"byte\": [ 1, 2, 3, 4 ] } } }",
			{ .u_descriminator = 0, .u = {.b = {.byte = {1, 2, 3, 4}}}});
	STRUCT_TYPE_JSON_POSITIVE(struct5_t,
			"{\"u\": {\"b\": {\"byte\": [1, 2, 3, 4]}}}",
			"{ \"u_descriminator\": \"0\", \"u\": { \"b\": { \"byte\": [ 1, 2, 3, 4 ] } } }",
			{ .u_descriminator = 0, .u = {.b = {.byte = {1, 2, 3, 4}}}});
	STRUCT_TYPE_JSON_POSITIVE(struct5_t,
			"{\"u_descriminator\": 1, \"u\": {\"w\": {\"word\": [1, 2]}}}",
			"{ \"u_descriminator\": \"1\", \"u\": { \"w\": { \"word\": [ \"1\", \"2\" ] } } }",
			{ .u_descriminator = 1, .u = {.w = {.word = {1, 2}}}});
	STRUCT_TYPE_JSON_POSITIVE(struct5_t,
			"{\"u\": {\"w\": {\"word\": [1, 2]}}}",
			"{ \"u_descriminator\": \"1\", \"u\": { \"w\": { \"word\": [ \"1\", \"2\" ] } } }",
			{ .u_descriminator = 1, .u = {.w = {.word = {1, 2}}}});
	STRUCT_TYPE_JSON_POSITIVE(struct5_t,
			"{\"u_descriminator\": 2, \"u\": {\"dw\": {\"dword\": [1]}}}",
			"{ \"u_descriminator\": \"2\", \"u\": { \"dw\": { \"dword\": [ \"1\" ] } } }",
			{ .u_descriminator = 2, .u = {.dw = {.dword = {1}}}});
	STRUCT_TYPE_JSON_POSITIVE(struct5_t,
			"{\"u\": {\"dw\": {\"dword\": [1]}}}",
			"{ \"u_descriminator\": \"2\", \"u\": { \"dw\": { \"dword\": [ \"1\" ] } } }",
			{ .u_descriminator = 2, .u = {.dw = {.dword = {1}}}});
	JSON_DES11N_NEGATIVE(struct5_t, "{\"u_descriminator\": 1, \"u\": {\"b\": {\"byte\": [1, 2, 3, 4]}}}");
	JSON_DES11N_NEGATIVE(struct5_t, "{\"u_descriminator\": 0, \"u\": {\"w\": {\"word\": [1, 2]}}}");
	JSON_DES11N_NEGATIVE(struct5_t, "{\"u_descriminator\": 0, \"u\": {\"b\": {\"byte\": [1, 2, 3, 4]}, \"w\": {\"word\": [1, 2]}}}");

}END_TEST

/*****************************************************************************/
typedef struct struct6 {
	int _descriminator;
	union {
		struct {
			uint8_t byte[4];
		}b;
		struct {
			uint16_t word[2];
		}w;
		struct {
			uint32_t dword[1];
		}dw;
	};
	union {
		struct {
			uint8_t byte[4];
		}b1;
		struct {
			uint16_t word[2];
		}w1;
		struct {
			uint32_t dword[1];
		}dw1;
	};
	struct {
		struct {
			uint8_t byte[4];
		}b2;
		struct {
			uint16_t word[2];
		}w2;
		struct {
			uint32_t dword[1];
		}dw2;
	};
}struct6_t;
METAC_TYPE_GENERATE(struct6_t);

START_TEST(ut_struct6_t){
	STRUCT_TYPE_JSON_POSITIVE(struct6_t,
			"{\"_descriminator\": 0, \"b\": {\"byte\": [1, 2, 3, 4]}}",
			"{ \"_descriminator\": \"0\", "
				"\"b\": { \"byte\": [ 1, 2, 3, 4 ] }, "
				"\"b1\": { \"byte\": [ 0, 0, 0, 0 ] }, "
				"\"b2\": { \"byte\": [ 0, 0, 0, 0 ] }, \"w2\": { \"word\": [ \"0\", \"0\" ] }, \"dw2\": { \"dword\": [ \"0\" ] } }",
			{ ._descriminator = 0, .b = {.byte = {1, 2, 3, 4}}});
	STRUCT_TYPE_JSON_POSITIVE(struct6_t,
			"{\"b\": {\"byte\": [1, 2, 3, 4]}}",
			"{ \"_descriminator\": \"0\", "
				"\"b\": { \"byte\": [ 1, 2, 3, 4 ] }, "
				"\"b1\": { \"byte\": [ 0, 0, 0, 0 ] }, "
				"\"b2\": { \"byte\": [ 0, 0, 0, 0 ] }, \"w2\": { \"word\": [ \"0\", \"0\" ] }, \"dw2\": { \"dword\": [ \"0\" ] } }",
			{ ._descriminator = 0, .b = {.byte = {1, 2, 3, 4}}});
	STRUCT_TYPE_JSON_POSITIVE(struct6_t,
			"{\"_descriminator\": 1, \"w\": {\"word\": [1, 2]}}",
			"{ \"_descriminator\": \"1\", "
			"\"w\": { \"word\": [ \"1\", \"2\" ] }, "
			"\"w1\": { \"word\": [ \"0\", \"0\" ] }, "
			"\"b2\": { \"byte\": [ 0, 0, 0, 0 ] }, \"w2\": { \"word\": [ \"0\", \"0\" ] }, \"dw2\": { \"dword\": [ \"0\" ] } }",
			{ ._descriminator = 1, .w = {.word = {1, 2}}});
	STRUCT_TYPE_JSON_POSITIVE(struct6_t,
			"{\"w\": {\"word\": [1, 2]}}",
			"{ \"_descriminator\": \"1\", "
			"\"w\": { \"word\": [ \"1\", \"2\" ] }, "
			"\"w1\": { \"word\": [ \"0\", \"0\" ] }, "
			"\"b2\": { \"byte\": [ 0, 0, 0, 0 ] }, \"w2\": { \"word\": [ \"0\", \"0\" ] }, \"dw2\": { \"dword\": [ \"0\" ] } }",
			{ ._descriminator = 1, .w = {.word = {1, 2}}});
	STRUCT_TYPE_JSON_POSITIVE(struct6_t,
			"{\"w\": {\"word\": [1, 2]}, \"w1\": {\"word\": [3, 4]}}",
			"{ \"_descriminator\": \"1\", "
			"\"w\": { \"word\": [ \"1\", \"2\" ] }, "
			"\"w1\": { \"word\": [ \"3\", \"4\" ] }, "
			"\"b2\": { \"byte\": [ 0, 0, 0, 0 ] }, \"w2\": { \"word\": [ \"0\", \"0\" ] }, \"dw2\": { \"dword\": [ \"0\" ] } }",
			{ ._descriminator = 1, .w = {.word = {1, 2}}, .w1 = {.word = {3, 4}}});
}END_TEST

/*****************************************************************************/

int main(void){
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(type_smoke){
					ADD_TEST(ut_char);
					ADD_TEST(ut_uchar_t);
					ADD_TEST(ut_short);
					ADD_TEST(ut_ushort_t);
					ADD_TEST(ut_int);
					ADD_TEST(ut_uint_t);
					ADD_TEST(ut_long);
					ADD_TEST(ut_ulong_t);
					ADD_TEST(ut_float);
					ADD_TEST(ut_double);
					ADD_TEST(ut_ldouble_t);
					ADD_TEST(ut_pchar_t);
					ADD_TEST(ut_enum_t);
					ADD_TEST(ut_struct1_t);
					ADD_TEST(ut_struct2_t);
					ADD_TEST(ut_bit_fields1_t);
					ADD_TEST(ut_bit_fields2_t);
					ADD_TEST(ut_char_array5_t);
					ADD_TEST(ut_pchar_array5_t);
					ADD_TEST(ut_struct3_t);
					ADD_TEST(ut_struct4_t);
					ADD_TEST(ut_struct5_t);
					ADD_TEST(ut_struct6_t);
			}END_CASE
			);
		}END_SUITE
	);
}
