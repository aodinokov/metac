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

#define BASIC_TYPE_JSON_DES11N_POSITIVE(_type_, _json_str_, _expected_value_) \
		do { \
			struct metac_object * res; \
			_type_ * res_data_ptr; \
			fail_unless((res = metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) != NULL, "metac_json2object returned NULL for %s", _json_str_); \
			res_data_ptr = (_type_*)res->ptr; \
			fail_unless((*res_data_ptr) == (_expected_value_), "unexpected data for %s", _json_str_); \
			fail_unless(metac_object_put(res) != 0, "Couldn't delete created object"); \
		}while(0); \
		mark_point();

#define BASIC_TYPE_JSON_DES11N_NEGATIVE(_type_, _json_str_) \
		do { \
			fail_unless((metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) == NULL, "metac_json2object must return NULL for %s", _json_str_); \
		}while(0); \
		mark_point();

START_TEST(basic_type_json_des11n){
	char buf[30];

	/*known issues:*/
	/* old versions of libjson trace for this tests:*/
//	BASIC_TYPE_JSON_DES11N_NEGATIVE(short, ".9");
//	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, ".9");
//	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, ".9");
	/* current version of libjson should return error for this tests:*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(short, "-7,9", -7); /*FIXME: looks like a bug in libjson*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "-7,9", -7); /*FIXME: looks like a bug in libjson*/
	/* current version of libjson truncates uint64_t to int64_t*/
//	snprintf(buf, sizeof(buf), "%lu", (long)UINT64_MAX);
//	BASIC_TYPE_JSON_DES11N_POSITIVE(ulong_t, buf, UINT64_MAX);
	/* truncated int data */
//	snprintf(buf, sizeof(buf), "%u", UINT32_MAX);
//	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, buf, UINT32_MAX);
//	snprintf(buf, sizeof(buf), "%ld", INT64_MIN);
//	BASIC_TYPE_JSON_DES11N_POSITIVE(long, buf, INT64_MIN);
//	snprintf(buf, sizeof(buf), "%ld", (long)INT64_MAX);
//	BASIC_TYPE_JSON_DES11N_POSITIVE(long, buf, INT64_MAX);
//	snprintf(buf, sizeof(buf), "%u", UINT64_MAX);
//	BASIC_TYPE_JSON_DES11N_POSITIVE(uint_t, buf, UINT64_MAX);

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
	/*enum_t*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eZero\"", _eZero);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eOne\"", _eOne);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eTen\"", _eTen);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eEleven\"", _eEleven);
	BASIC_TYPE_JSON_DES11N_POSITIVE(enum_t, "\"_eTwelve\"", _eTwelve);
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "\"_eZerox\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "\"x_eZero\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "0");	/*FIXME: we can allow int values that present in enum*/
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, "{\"xx\": \"xy\"}");

}END_TEST

typedef char char_t;
typedef long long int llongint_t;
typedef char_t char_array5_t[5];
METAC_TYPE_GENERATE(char_array5_t);

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

typedef struct _struct1_
{
  unsigned int x;
  unsigned int y;
}struct1_t;

typedef struct _struct2_
{
	struct1_t * str1;
}struct2_t;
METAC_TYPE_GENERATE(struct2_t);


START_TEST(metac_json_deserialization) {
	struct metac_object * res;
	do {
		char_array5_t *pres;
		char_array5_t eres = {'a', 'b', 'c', 'd', 'e'};
		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(char_array5_t), "[\"a\", \"b\",\"c\",\"d\",\"e\",]")) != NULL,
				"metac_json2object returned NULL");
		pres = (char_array5_t*)res->ptr;
		fail_unless(memcmp(pres, &eres, sizeof(eres)) == 0, "unexpected data");
		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
	}while(0);
	mark_point();

/*
	typedef struct _bit_fields_
	{
	  unsigned int widthValidated : 9;
	  unsigned int heightValidated : 12;
	}bit_fields_t;
	METAC_TYPE_GENERATE(bit_fields_t);
*/


	do {
		int i;
		unsigned char * p;
		bit_fields_t *pres;
		bit_fields_t eres = {.widthValidated = 6, .heightValidated = 100};
		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(bit_fields_t), "{\"widthValidated\": 6, \"heightValidated\": 100}")) != NULL,
				"metac_json2object returned NULL");
		pres = (bit_fields_t*)res->ptr;
/*
printf("result:\n");
p = (unsigned char *) pres;
for (i=0; i<sizeof(bit_fields_t); i++){
printf("%02x ", (int)*p);
p++;
}
printf("\n");
printf("expected:\n");
p = (unsigned char *) &eres;
for (i=0; i<sizeof(bit_fields_t); i++){
printf("%02x ", (int)*p);
p++;
}
printf("\n");
*/

		fail_unless(
				pres->widthValidated == eres.widthValidated &&
				pres->heightValidated == eres.heightValidated, "unexpected data");
		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
	}while(0);
	mark_point();

/*
typedef struct _bit_fields_for_longer_than32_bit
{
  unsigned int widthValidated : 9;
  unsigned int heightValidated : 12;
  int heightValidated1 : 30;
  llongint_t heightValidated2 : 63;
}bit_fields_for_longer_than32_bit_t;
METAC_TYPE_GENERATE(bit_fields_for_longer_than32_bit_t);
 */
	do {
		int i;
		unsigned char * p;
		bit_fields_for_longer_than32_bit_t *pres;
		bit_fields_for_longer_than32_bit_t eres = {
				.widthValidated = 6,
				.heightValidated = 100,
				.heightValidated1 = -1,
				.heightValidated2 = 10000000};
		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(bit_fields_for_longer_than32_bit_t),
				"{\"widthValidated\": 6, \"heightValidated\": 100, \"heightValidated1\": -1, \"heightValidated2\": 10000000}")) != NULL,
				"metac_json2object returned NULL");
		pres = (bit_fields_for_longer_than32_bit_t*)res->ptr;
//printf("result:\n");
//p = (unsigned char *) pres;
//for (i=0; i<sizeof(bit_fields_for_longer_than32_bit_t); i++){
//printf("%02x ", (int)*p);
//p++;
//}
//printf("\n");
//printf("expected:\n");
//p = (unsigned char *) &eres;
//for (i=0; i<sizeof(bit_fields_for_longer_than32_bit_t); i++){
//printf("%02x ", (int)*p);
//p++;
//}
//printf("\n");
		fail_unless(
				pres->widthValidated == eres.widthValidated &&
				pres->heightValidated == eres.heightValidated /*&&
				pres->heightValidated1 == eres.heightValidated1 &&
				pres->heightValidated2 == eres.heightValidated2*/
				, "unexpected data");
		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
	}while(0);
	mark_point();



	/*nedative fail_unless((res = metac_json2object(&METAC_TYPE_NAME(char_array5_t), "[\"a\", \"b\",\"c\",\"d\",\"e\",\"f\",]")) != NULL,
	 * 		"metac_json2object returned NULL");*/
	do {
		struct2_t *pres;
		struct1_t str1 = {.x = 1, .y = 8};
		struct2_t eres = {.str1 = &str1};

		fail_unless((res = metac_json2object(&METAC_TYPE_NAME(struct2_t), "{\"str1\":{\"x\": 1, \"y\": 8}}")) != NULL,
				"metac_json2object returned NULL");
		pres = (struct2_t*)res->ptr;
		fail_unless(pres->str1 != NULL, "pointer wasn't initialized");
		fail_unless(
				pres->str1->x == eres.str1->x &&
				pres->str1->y == eres.str1->y, "unexpected data");
		fail_unless(metac_object_put(res) != 0, "Couldn't delete created object");
	}while(0);
	mark_point();

}END_TEST

int main(void){
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(type_smoke){
					ADD_TEST(basic_type_json_des11n);
					ADD_TEST(metac_json_deserialization);
				}END_CASE
			);
		}END_SUITE
	);
}
