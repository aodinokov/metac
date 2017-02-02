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

/*serialization - move to another file*/
struct metac_object * metac_json2object(struct metac_type * mtype, char *string);

METAC_TYPE_GENERATE(char);

METAC_TYPE_GENERATE(int);

typedef int int_t;
METAC_TYPE_GENERATE(int_t);

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
			fail_unless((res = metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) != NULL, "metac_json2object returned NULL"); \
			res_data_ptr = (_type_*)res->ptr; \
			fail_unless(*res_data_ptr == _expected_value_, "unexpected data"); \
			fail_unless(metac_object_put(res) != 0, "Couldn't delete created object"); \
		}while(0); \
		mark_point();

#define BASIC_TYPE_JSON_DES11N_NEGATIVE(_type_, _json_str_) \
		do { \
			fail_unless((metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) == NULL, "metac_json2object must return NULL"); \
		}while(0); \
		mark_point();

START_TEST(basic_type_json_des11n){
	char buf[30];
	/*char*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"c\"", 'c');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"0\"", '0');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"A\"", 'A');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"z\"", 'z');
	BASIC_TYPE_JSON_DES11N_POSITIVE(char, "\"&\"", '&');
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "7");		/*FIXME: we can allow int values in range 0-255*/
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(char, "{\"xx\": \"xy\"}");
	/*int*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "-10", -10);
	snprintf(buf, sizeof(buf), "%d", INT32_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, buf, INT32_MAX);
	snprintf(buf, sizeof(buf), "%d", INT32_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, buf, INT32_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int, "-7,9", -7); /*FIXME: looks like a bug in libjson*/
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, ".9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int, "{\"xx\": \"xy\"}");
	/*int_t*/
	BASIC_TYPE_JSON_DES11N_POSITIVE(int_t, "0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int_t, "-0", 0);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int_t, "1234", 1234);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int_t, "-10", -10);
	snprintf(buf, sizeof(buf), "%d", INT32_MAX);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int_t, buf, INT32_MAX);
	snprintf(buf, sizeof(buf), "%d", INT32_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int_t, buf, INT32_MIN);
	BASIC_TYPE_JSON_DES11N_POSITIVE(int_t, "-7,9", -7); /*FIXME: looks like a bug in libjson*/
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int_t, "\"c\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int_t, "\"cc\"");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int_t, "-7.9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int_t, ".9");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int_t, "[\"xx\", \"xy\"]");
	BASIC_TYPE_JSON_DES11N_NEGATIVE(int_t, "{\"xx\": \"xy\"}");
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
	BASIC_TYPE_JSON_DES11N_NEGATIVE(enum_t, ".9");
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
