/*
 * metac_s11n_json_ut_002.c
 *
 *  Created on: Nov 5, 2017
 *      Author: mralex
 */

#include "check_ext.h"
#include "metac_type.h"

#include <stdint.h>	/*INT32_MIN, INT32_MAX*/
#include <stdio.h>	/*sprintf*/
#include <string.h> /*strdupa*/

/*serialization - TBD!!! move to another file*/
struct metac_object * metac_json2object(struct metac_type * mtype, char *string);
char * metac_type_and_ptr2json_string(struct metac_type * type, void * ptr, metac_byte_size_t byte_size);

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
#define JSON_POSITIVE_BEGIN(_type_, _json0_, _json1_, _expected_data_...) do { \
		int i; \
		static _type_ expected_data = _expected_data_; \
		struct metac_object * cdata, * _cdata[2]; \
		char * json, * _json[2] = {_json0_, _json1_}; \
		char * json1, *_json1; \
		_cdata[0] = metac_json2object(&METAC_TYPE_NAME(_type_), _json0_); \
		_json1 =  metac_type_and_ptr2json_string(&METAC_TYPE_NAME(_type_), &expected_data, sizeof(expected_data)); \
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

#define JSON_POSITIVE_END \
			fail_unless(metac_object_put(cdata) != 0, "Couldn't delete created object cdata%d", i); \
		}\
	}while(0);\
	mark_point();

/* special cases for positive UTs */
#define BASIC_TYPE_JSON_POSITIVE(_type_, _json0_, _json1_, _expected_data_...) \
	JSON_POSITIVE_BEGIN(_type_, _json0_, _json1_, _expected_data_) \
	fail_unless((*((_type_*)cdata->ptr)) == (_expected_data_), "Des11n: unexpected data for %s: %x, expected %s", json, (int) (*((_type_*)cdata->ptr)), #_expected_data_); \
	JSON_POSITIVE_END

#define PCHAR_TYPE_JSON_POSITIVE(_type_, _json0_, _json1_, _expected_data_...) \
	JSON_POSITIVE_BEGIN(_type_, _json0_, _json1_, _expected_data_) \
	fail_unless(strcmp((*((_type_*)cdata->ptr)), (expected_data)) == 0, "Des11n: unexpected data for %s", json); \
	JSON_POSITIVE_END

#define STRUCT_TYPE_JSON_POSITIVE(_type_, _json0_, _json1_, _expected_data_...) \
	JSON_POSITIVE_BEGIN(_type_, _json0_, _json1_, _expected_data_) \
	if (memcmp((_type_*)cdata->ptr, &expected_data, sizeof(expected_data))!=0) { \
		DUMP_MEM("data         : ", cdata->ptr, sizeof(expected_data)); \
		DUMP_MEM("expected data: ", &expected_data, sizeof(expected_data)); \
		fail_unless(0, "Des11n: unexpected data for %s, expected %s", json, #_expected_data_); \
	JSON_POSITIVE_END

#define ARRAY_TYPE_JSON_POSITIVE STRUCT_TYPE_JSON_POSITIVE

/*---------------*/
#define JSON_S11N_NEGATIVE_WITH_LEN(_type_, _data_len_, _data_...) \
		do { \
			static _type_ data = _data_; \
			char * _json1 =  metac_type_and_ptr2json_string(&METAC_TYPE_NAME(_type_), &data, _data_len_); \
			char * json1 = _json1?strdupa(_json1):NULL; free(_json1); _json1= NULL; \
			fail_unless(json1 == NULL, "metac_type_and_ptr2json_string must return NULL, but returned %s", json1); \
		}while(0); \
		mark_point();
#define JSON_S11N_NEGATIVE(_type_, _data_...) JSON_S11N_NEGATIVE(_type_, sizeof(_data_), _data_)

#define JSON_DES11N_NEGATIVE(_type_, _json_str_) \
		do { \
			fail_unless((metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) == NULL, "metac_json2object must return NULL for %s", _json_str_); \
		}while(0); \
		mark_point();

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
//					ADD_TEST(structure_type_json);
//					ADD_TEST(array_type_json);
//					ADD_TEST(union_type_json);
			}END_CASE
			);
		}END_SUITE
	);
}

