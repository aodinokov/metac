/*
 * metac_s11n_json_ut_001.c
 *
 *  Created on: Feb 2, 2017
 *      Author: mralex
 */


#include "check_ext.h"
#include "metac_type.h"
#include <stdint.h>	/*INT32_MIN, INT32_MAX*/
#include <stdio.h>	/*sprintf*/

#include <json/json.h>	/*to get version of lib*/


/*serialization - move to another file*/
struct metac_object * metac_json2object(struct metac_type * mtype, char *string);

/*
 * UT helper macros
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

#define JSON_DES11N_POSITIVE_START(_type_, _json_str_, _expected_value_...) \
		do { \
			struct metac_object * res; \
			_type_	expected_value = _expected_value_; \
			fail_unless((res = metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) != NULL, "metac_json2object returned NULL for %s", _json_str_); \
			fail_unless(res->ptr != NULL, "Ptr is NULL");

#define JSON_DES11N_POSITIVE_END() \
			fail_unless(metac_object_put(res) != 0, "Couldn't delete created object"); \
		}while(0); \
		mark_point();

#define BASIC_TYPE_JSON_DES11N_NEGATIVE(_type_, _json_str_) \
		do { \
			fail_unless((metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) == NULL, "metac_json2object must return NULL for %s", _json_str_); \
		}while(0); \
		mark_point();

/*
 * Generate meta-info for some basic types and their typedefs
 * and perform some UTs
 */
#define BASIC_TYPE_JSON_DES11N_POSITIVE(_type_, _json_str_, _expected_value_...) \
		JSON_DES11N_POSITIVE_START(_type_, _json_str_, _expected_value_) \
		fail_unless((*((_type_*)res->ptr)) == (expected_value), "unexpected data for %s, expected %s", _json_str_, #_expected_value_); \
		JSON_DES11N_POSITIVE_END()

#define PCHAR_TYPE_JSON_DES11N_POSITIVE(_type_, _json_str_, _expected_value_) \
		JSON_DES11N_POSITIVE_START(_type_, _json_str_, _expected_value_) \
		fail_unless(strcmp((*((_type_*)res->ptr)), (expected_value)) == 0, "unexpected data for %s", _json_str_); \
		JSON_DES11N_POSITIVE_END()

METAC_TYPE_GENERATE(char);
METAC_TYPE_GENERATE(short);
METAC_TYPE_GENERATE(int);
METAC_TYPE_GENERATE(long);
typedef unsigned char uchar_t;
METAC_TYPE_GENERATE(uchar_t);
typedef unsigned short ushort_t;
METAC_TYPE_GENERATE(ushort_t);
typedef unsigned int uint_t;
METAC_TYPE_GENERATE(uint_t);
typedef unsigned long ulong_t;
METAC_TYPE_GENERATE(ulong_t);
METAC_TYPE_GENERATE(float);
METAC_TYPE_GENERATE(double);
typedef long double ldouble_t;
METAC_TYPE_GENERATE(ldouble_t);
typedef enum _enum_{
	_eZero = 0,
	_eOne,
	_eTen = 10,
	_eEleven,
	_eTwelve,
}enum_t;
METAC_TYPE_GENERATE(enum_t);
typedef char* pchar_t;
METAC_TYPE_GENERATE(pchar_t);

START_TEST(basic_type_json_des11n){
	char buf[30];
	/*known issues:*/
	/* old versions of libjson trace for this tests:*/
#if JSON_C_MAJOR_VERSION > 0 || JSON_C_MINOR_VERSION >= 10
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, ".9");
#endif
	/* current version of libjson should return error for this tests:*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "-7,9", -7); /*FIXME: looks like a bug in libjson*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "-7,9", -7); /*FIXME: looks like a bug in libjson*/
	/* current version of libjson truncates uint64_t to int64_t (and older truncates even more - see below)*/
	/*snprintf(buf, sizeof(buf), "%lu", (long)UINT64_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, buf, UINT64_MAX);*/

	/*char*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"c\"", 'c');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"0\"", '0');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"A\"", 'A');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"z\"", 'z');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"&\"", '&');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "7", 0x7);
	snprintf(buf, sizeof(buf), "%d", INT8_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, buf, INT8_MIN);
	snprintf(buf, sizeof(buf), "%d", INT8_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, buf, INT8_MAX);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "{\"xx\": \"xy\"}");
	/*uchar_t*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(uchar_t, "\"c\"", 'c');
	BASIC_TYPE_JSON_DES11N_POSITIVE(uchar_t, "\"0\"", '0');
	BASIC_TYPE_JSON_DES11N_POSITIVE(uchar_t, "\"A\"", 'A');
	BASIC_TYPE_JSON_DES11N_POSITIVE(uchar_t, "\"z\"", 'z');
	BASIC_TYPE_JSON_DES11N_POSITIVE(uchar_t, "\"&\"", '&');
	BASIC_TYPE_JSON_DES11N_POSITIVE(uchar_t, "7", 0x7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(uchar_t, "0", 0);
	snprintf(buf, sizeof(buf), "%ld", (long)UINT8_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(uchar_t, buf, UINT8_MAX);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uchar_t, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uchar_t, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uchar_t, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uchar_t, "{\"xx\": \"xy\"}");
	/*short*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "-10", -10);
	snprintf(buf, sizeof(buf), "%ld", (long)INT16_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, buf, INT16_MAX);
	snprintf(buf, sizeof(buf), "%ld", (long)INT16_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, buf, INT16_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"-7\"", -7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"-0\"", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"-07\"", -07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"07\"", 07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"076234\"", 076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"-076234\"", -076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"-0xa\"", -0xa);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"0x7fff\"", 0x7fff);
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "\"-0x8000\"", -0x8000);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, "{\"xx\": \"xy\"}");
	/*ushort_t*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(ushort_t, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ushort_t, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ushort_t, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ushort_t, "0", 0);
	snprintf(buf, sizeof(buf), "%ld", (long)UINT16_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ushort_t, buf, UINT16_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ushort_t, "\"07\"", 07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ushort_t, "\"076234\"", 076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ushort_t, "\"0xffff\"", 0xffff);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-7\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-07\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-076234\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-0xa\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-0x8000\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ushort_t, "{\"xx\": \"xy\"}");
	/*int*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "-10", -10);
	snprintf(buf, sizeof(buf), "%d", INT32_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, buf, INT32_MIN);
	snprintf(buf, sizeof(buf), "%d", INT32_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, buf, INT32_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"-7\"", -7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"-0\"", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"-07\"", -07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"07\"", 07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"076234\"", 076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"-076234\"", -076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"-0xa\"", -0xa);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"0x7fffffff\"", 0x7fffffff);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "\"-0x80000000\"", -0x80000000);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "{\"xx\": \"xy\"}");
	/*uint_t*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, "0", 0);
#if JSON_C_MAJOR_VERSION > 0 || JSON_C_MINOR_VERSION >= 10
	snprintf(buf, sizeof(buf), "%u", UINT32_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, buf, UINT32_MAX);
#endif
	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, "\"07\"", 07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, "\"076234\"", 076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, "\"0xffffffff\"", 0xffffffff);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "-10");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-7\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-07\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-076234\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-0xa\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-0x80000000\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(uint_t, "{\"xx\": \"xy\"}");
	/*long*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "-10", -10);
#if JSON_C_MAJOR_VERSION > 0 || JSON_C_MINOR_VERSION >= 10
	snprintf(buf, sizeof(buf), "%ld", INT64_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, buf, INT64_MIN);
	snprintf(buf, sizeof(buf), "%ld", (long)INT64_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, buf, INT64_MAX);
#endif
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"-7\"", -7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"-0\"", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"-07\"", -07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"07\"", 07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"076234\"", 076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"-076234\"", -076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"-0xa\"", -0xa);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"0x7fffffff\"", 0x7fffffff);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"-0x80000000\"", -0x80000000L);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"0x7fffffffffffffff\"", 0x7fffffffffffffffL);
	BASIC_TYPE_JSON_DES11N_POSITIVE(long, "\"-0x8000000000000000\"", -0x8000000000000000L);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(long, "{\"xx\": \"xy\"}");
	/*ulong_t*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, "\"07\"", 07);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, "\"076234\"", 076234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, "\"0xffffffff\"", 0xffffffff);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, "\"0xffffffffffffffff\"", 0xffffffffffffffffL);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "-10");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-7\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-07\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-076234\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-0xa\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-0x80000000\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ulong_t, "{\"xx\": \"xy\"}");
	/*float*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "0.7", 0.7f);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "-0.7", -0.7f);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "\"0.7\"", 0.7f);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "\"0.76234\"", 0.76234f);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "\"-1024.934\"", -1024.934f);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "\"-07\"", -07.0f);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "\"-076234\"", -076234.0f);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "\"-0xa.0\"", -10.0f);
	BASIC_TYPE_JSON_DES11N_POSITIVE(float, "\"-0x80000000\"", -2147483648.0f);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(float, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(float, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(float, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(float, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(float, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(float, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(float, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(float, "{\"xx\": \"xy\"}");
	/*double*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "0.7", 0.7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "-0.7", -0.7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "\"0.7\"", 0.7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "\"0.76234\"", 0.76234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "\"-1024.934\"", -1024.934);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "\"-07\"", -07.0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "\"-076234\"", -076234.0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "\"-0xa.0\"", -10.0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(double, "\"-0x80000000\"", -2147483648.0);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(double, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(double, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(double, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(double, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(double, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(double, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(double, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(double, "{\"xx\": \"xy\"}");
	/*ldouble_t*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "1234", 1234);
	/* For the next 2 tests:
	 * 0.7L/-0.7L will not work because libjson returns double and we convert it to long double
	 * and it will not be equal to 0.7L (some difference in lower bits)
	 */
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "0.7", 0.7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "-0.7", -0.7);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "\"0.7\"", 0.7L);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "\"0.76234\"", 0.76234L);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "\"-1024.934\"", -1024.934L);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "\"-07\"", -07.0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "\"-076234\"", -076234.0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "\"-0xa.0\"", -10.0L);
	BASIC_TYPE_JSON_DES11N_POSITIVE(ldouble_t, "\"-0x80000000\"", -2147483648.0);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ldouble_t, "\"-0a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ldouble_t, "\"-01a\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ldouble_t, "\"-0x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ldouble_t, "\"-x\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ldouble_t, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ldouble_t, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ldouble_t, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(ldouble_t, "{\"xx\": \"xy\"}");
	/*enum_t*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eZero\"", _eZero);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eOne\"", _eOne);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eTen\"", _eTen);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eEleven\"", _eEleven);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eTwelve\"", _eTwelve);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "0", _eZero);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "1", _eOne);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "10", _eTen);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "11", _eEleven);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "12", _eTwelve);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "13");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "\"_eZerox\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "\"x_eZero\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "{\"xx\": \"xy\"}");
	/*pchar_t*/
	PCHAR_TYPE_JSON_DES11N_POSITIVE(pchar_t, "\"string1234\"", "string1234");
	PCHAR_TYPE_JSON_DES11N_POSITIVE(pchar_t, "[\"s\", \"t\", \"r\", \"i\", \"n\", \"g\", \"1\", \"2\", \"3\" ,\"4\", 0]", "string1234");

}END_TEST

/*
 * Generate meta-info for some struct types and their typedefs
 * and perform some UTs
 */

#define STRUCT_TYPE_JSON_DES11N_POSITIVE(_type_, _json_str_, _expected_value_...) \
		JSON_DES11N_POSITIVE_START(_type_, _json_str_, _expected_value_) \
		if (memcmp((_type_*)res->ptr, &expected_value, sizeof(expected_value))!=0) { \
			DUMP_MEM("data         : ", res->ptr, sizeof(expected_value)); \
			DUMP_MEM("expected data: ", &expected_value, sizeof(expected_value)); \
			fail_unless(0, "unexpected data for %s, expected %s", _json_str_, #_expected_value_); \
		}\
		JSON_DES11N_POSITIVE_END()

#define STRUCT_TYPE_JSON_DES11N_NEGATIVE BASIC_TYPE_JSON_DES11N_NEGATIVE

typedef struct struct1 {
	int x;
	unsigned int y;
}struct1_t;
METAC_TYPE_GENERATE(struct1_t);
typedef struct bit_fields1 {
	unsigned int field_15b: 15;
	unsigned int field_17b: 17;
}bit_fields1_t;
METAC_TYPE_GENERATE(bit_fields1_t);
typedef struct bit_fields2 {
	unsigned int field_9b: 9;
	unsigned int field_12b: 12;
	long field_36b: 36;
	long field_63b: 63;
}bit_fields2_t;
METAC_TYPE_GENERATE(bit_fields2_t);
typedef struct struct2 {
	struct1_t * str1;
}struct2_t;
METAC_TYPE_GENERATE(struct2_t);

START_TEST(structure_type_json_des11n) {
	/*struct1_t*/
	STRUCT_TYPE_JSON_DES11N_POSITIVE(struct1_t, "{}", {});
	STRUCT_TYPE_JSON_DES11N_POSITIVE(struct1_t, "{\"x\": -1}", {.x = -1});
	STRUCT_TYPE_JSON_DES11N_POSITIVE(struct1_t, "{\"y\": 1}", {.y = 1});
	STRUCT_TYPE_JSON_DES11N_POSITIVE(struct1_t, "{\"x\": -1, \"y\": 1}", {.x = -1, .y = 1});
	STRUCT_TYPE_JSON_DES11N_NEGATIVE(struct1_t, "{\"y\": -1}");
	/*bit_fields1_t*/
	STRUCT_TYPE_JSON_DES11N_POSITIVE(bit_fields1_t, "{}", {});
	STRUCT_TYPE_JSON_DES11N_POSITIVE(bit_fields1_t, "{\"field_15b\": 6}", {.field_15b = 6});
	STRUCT_TYPE_JSON_DES11N_POSITIVE(bit_fields1_t, "{\"field_17b\": 6000}", {.field_17b = 6000});
	STRUCT_TYPE_JSON_DES11N_POSITIVE(bit_fields1_t, "{\"field_15b\": 6, \"field_17b\": 100}",
			{.field_15b = 6, .field_17b = 100});
	STRUCT_TYPE_JSON_DES11N_POSITIVE(bit_fields1_t, "{\"field_15b\": 16384, \"field_17b\": 65536}",
			{.field_15b = 16384, .field_17b = 65536});
	STRUCT_TYPE_JSON_DES11N_NEGATIVE(bit_fields1_t, "{\"field_15b\": -6}");
	/*bit_fields2_t*/
	STRUCT_TYPE_JSON_DES11N_POSITIVE(bit_fields2_t, "{\"field_9b\": 6, \"field_12b\": 100, \"field_36b\": -1, \"field_63b\": 1000}",
			{.field_9b = 6, .field_12b = 100, .field_36b = -1, .field_63b = 1000});
	/*struct2_t*/ {
		struct1_t str1 = {.x = 1, .y = 8};
		struct2_t eres = {.str1 = &str1};
		JSON_DES11N_POSITIVE_START(struct2_t, "{\"str1\":{\"x\": 1, \"y\": 8}}", {}) {
			struct2_t * pres = (struct2_t*)res->ptr;
			fail_unless(
					pres->str1->x == eres.str1->x &&
					pres->str1->y == eres.str1->y, "unexpected data");
		}JSON_DES11N_POSITIVE_END();
	}
	STRUCT_TYPE_JSON_DES11N_NEGATIVE(struct2_t, "{\"str1\":\"string\"}");
}END_TEST

typedef char char_array5_t[5];
METAC_TYPE_GENERATE(char_array5_t);
typedef char* pchar_array5_t[5];
METAC_TYPE_GENERATE(pchar_array5_t);
/*flexible array - pattern 1 - with null last element*/
typedef struct struct3 {
	int x;
	char flex_arr3[];
}struct3_t;
METAC_TYPE_GENERATE(struct3_t);
/*flexible array - pattern 2 - with <name>_len basic_type sibling with singed/unsigned encoding*/
typedef struct struct4 {
	int flex_arr4_len;
	char flex_arr4[];
}struct4_t;
METAC_TYPE_GENERATE(struct4_t);

#define ARRAY_TYPE_JSON_DES11N_POSITIVE STRUCT_TYPE_JSON_DES11N_POSITIVE
#define ARRAY_TYPE_JSON_DES11N_NEGATIVE STRUCT_TYPE_JSON_DES11N_NEGATIVE

START_TEST(array_type_json_des11n) {
	/*char_array5_t*/
	ARRAY_TYPE_JSON_DES11N_POSITIVE(char_array5_t, "[\"a\",\"b\",\"c\",\"d\",\"e\"]", {'a', 'b', 'c', 'd', 'e'});
	ARRAY_TYPE_JSON_DES11N_NEGATIVE(char_array5_t, "[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\"]");
	/*pchar_array5_t*/
	JSON_DES11N_POSITIVE_START(pchar_array5_t, "[\"a\",\"b\",\"c\",\"d\",\"e\"]", {"a", "b", "c", "d", "e"}) {
		int i;
		for (i = 0; i < sizeof(expected_value)/sizeof(expected_value[0]); ++i) {
			char * element = (*((pchar_array5_t*)res->ptr))[i];
			fail_unless(strcmp(element, expected_value[i]) == 0, "got incorrect value %s, expected %s", element, expected_value[i]);
		}
	}JSON_DES11N_POSITIVE_END();
	/*struct3_t*/
	{
		static struct3_t expected_struct3 = {.flex_arr3 = {'a', 'b', 'c', 'd', 'e', 0}};
		JSON_DES11N_POSITIVE_START(struct3_t, "{\"flex_arr3\": [\"a\", \"b\", \"c\", \"d\", \"e\"]}", {}) {
			int i;
			struct3_t * _struct3 = (struct3_t*)res->ptr;
			for (i = 0; i < 6/*sizeof(expected_value)/sizeof(expected_value[0])*/; ++i) {
				fail_unless(_struct3->flex_arr3[i] == expected_struct3.flex_arr3[i], "got incorrect value %c, expected %c",
						_struct3->flex_arr3[i], expected_struct3.flex_arr3[i]);
			}
		}JSON_DES11N_POSITIVE_END();
	}
	/*TBD: ut with 0 elements */

	/*struct4_t*/
	{
		static struct4_t expected_struct4 = {.flex_arr4_len = 5, .flex_arr4 = {'a', 'b', 'c', 'd', 'e'}};
		JSON_DES11N_POSITIVE_START(struct4_t, "{\"flex_arr4_len\": 5, \"flex_arr4\": [\"a\", \"b\", \"c\", \"d\", \"e\"]}", {}) {
			int i;
			struct4_t * _struct4 = (struct4_t*)res->ptr;
			fail_unless(_struct4->flex_arr4_len == expected_struct4.flex_arr4_len, "Len must be equal %d, %d",
					(int)_struct4->flex_arr4_len, (int)expected_struct4.flex_arr4_len);
			for (i = 0; i < _struct4->flex_arr4_len; ++i) {
				fail_unless(_struct4->flex_arr4[i] == expected_struct4.flex_arr4[i], "got incorrect value %c, expected %c",
						_struct4->flex_arr4[i], expected_struct4.flex_arr4[i]);
			}
		}JSON_DES11N_POSITIVE_END();
	}
	/*struct4_t case with self initialized flex_arr4_len if present*/
	{
		static struct4_t expected_struct4 = {.flex_arr4_len = 5, .flex_arr4 = {'a', 'b', 'c', 'd', 'e'}};
		JSON_DES11N_POSITIVE_START(struct4_t, "{\"flex_arr4\": [\"a\", \"b\", \"c\", \"d\", \"e\"]}", {}) {
			int i;
			struct4_t * _struct4 = (struct4_t*)res->ptr;
			fail_unless(_struct4->flex_arr4_len == expected_struct4.flex_arr4_len, "Len must be equal %d, %d",
					(int)_struct4->flex_arr4_len, (int)expected_struct4.flex_arr4_len);
			for (i = 0; i < _struct4->flex_arr4_len; ++i) {
				fail_unless(_struct4->flex_arr4[i] == expected_struct4.flex_arr4[i], "got incorrect value %c, expected %c",
						_struct4->flex_arr4[i], expected_struct4.flex_arr4[i]);
			}
		}JSON_DES11N_POSITIVE_END();
	}


}END_TEST

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

typedef struct struct6 {
	int descriminator;
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


#define UNION_TYPE_JSON_DES11N_POSITIVE STRUCT_TYPE_JSON_DES11N_POSITIVE
#define UNION_TYPE_JSON_DES11N_NEGATIVE STRUCT_TYPE_JSON_DES11N_NEGATIVE

START_TEST(union_type_json_des11n) {
	/*struct5_t*/
	UNION_TYPE_JSON_DES11N_POSITIVE(struct5_t,
			"{\"u_descriminator\": 0, \"u\": {\"b\": {\"byte\": [1, 2, 3, 4]}}}",
			{ .u_descriminator = 0, .u = {.b = {.byte = {1, 2, 3, 4}}}});
	UNION_TYPE_JSON_DES11N_POSITIVE(struct5_t,
			"{\"u\": {\"w\": {\"word\": [1, 2]}}}",
			{ .u_descriminator = 0/*FIXME: must be 1*/, .u = {.w = {.word = {1, 2}}}});

	BASIC_TYPE_JSON_DES11N_NEGATIVE(struct5_t, "{\"u_descriminator\": 0, \"u\": {\"b\": {\"byte\": [1, 2, 3, 4]}, \"w\": {\"word\": [1, 2]}}}");
	/*struct6_t*/
	UNION_TYPE_JSON_DES11N_POSITIVE(struct6_t,
			"{\"descriminator\": 0, \"b\": {\"byte\": [1, 2, 3, 4]}}",
			{ .descriminator = 0, .b = {.byte = {1, 2, 3, 4}}});

}END_TEST

int main(void){
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(type_smoke){
					ADD_TEST(basic_type_json_des11n);
					ADD_TEST(structure_type_json_des11n);
					ADD_TEST(array_type_json_des11n);
					ADD_TEST(union_type_json_des11n);
				}END_CASE
			);
		}END_SUITE
	);
}
