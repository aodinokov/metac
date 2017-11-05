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
/*TBD: to think about vvv*/
#define JSON_S11N_NEGATIVE(_type_, _data_...) \
		do { \
			static _type_ data = _data_; \
			char * _json1 =  metac_type_and_ptr2json_string(&METAC_TYPE_NAME(_type_), &data, sizeof(data)); \
			char * json1 = _json1?strdupa(_json1):NULL; free(_json1); _json1= NULL; \
			fail_unless(json1 == NULL, "metac_type_and_ptr2json_string must return NULL, but returned %s", json1); \
		}while(0); \
		mark_point();

#define JSON_DES11N_NEGATIVE(_type_, _json_str_) \
		do { \
			fail_unless((metac_json2object(&METAC_TYPE_NAME(_type_), _json_str_)) == NULL, "metac_json2object must return NULL for %s", _json_str_); \
		}while(0); \
		mark_point();

/*---------------*/

METAC_TYPE_GENERATE(char);

START_TEST(basic_type_json){
	char buf[30];
	/*char*/
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
}END_TEST

int main(void){
	return run_suite(
		START_SUITE(type_suite){
			ADD_CASE(
				START_CASE(type_smoke){
					ADD_TEST(basic_type_json);
//					ADD_TEST(structure_type_json);
//					ADD_TEST(array_type_json);
//					ADD_TEST(union_type_json);
			}END_CASE
			);
		}END_SUITE
	);
}

