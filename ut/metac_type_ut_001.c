/*
 * metac_ut_001.c
 *
 *  Created on: Oct 3, 2015
 *      Author: mralex
 */

//#define METAC_DEBUG_ENABLE
#include <dlfcn.h>
#include <stdarg.h>
#include <complex.h>		/*complex*/

#include "debug.h"  /* msg_stderr, ...*/
#include "metac/base.h"

#include "check_ext.h"

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
#if !defined(__clang__) || (__clang_major__ == 3 && __clang_minor__ > 4) || (__clang_major__ > 3)
#define _BUG_NO_USPECIFIED_PARAMETER_ 0
#else
#define _BUG_NO_USPECIFIED_PARAMETER_ 1
#endif

/* 3. Some compilers generate dwarf data without count/upper/lower_bound for arrays with len=0. It's a bug.
 * gcc before 9.0 and clang 3.5.0 has this issue, but 3.9.0 - doesn't
 */
#if defined(__clang__) && (( __clang_major__ > 3 ) || ( __clang_major__ == 3 && __clang_minor__ >= 9 && __clang_patchlevel__ >=0 ))
#define _BUG_ZERO_LEN_IS_FLEXIBLE_ 0
#else
#if defined(__GNUC__) && (__GNUC_PREREQ(9,0))
#define _BUG_ZERO_LEN_IS_FLEXIBLE_ 0
#else
#define _BUG_ZERO_LEN_IS_FLEXIBLE_ 1
#endif
#endif
/*
 * TBD: autodetection issue:
 https://travis-ci.org/aodinokov/metac/jobs/343649274
 $ clang --version
 clang version 5.0.0 (tags/RELEASE_500/final)
 bug_zero_len_is_flexible 1
 bug_with_unspecified_parameters 1
 *
 * */
/*****************************************************************************/
static int _dummy_discriminator(char *annotation_key, int write_operation, /* 0 - if need to store date to p_discriminator_val, 1 - vice-versa*/
void *ptr, metac_type_t *type, /*pointer to memory region and its type */
metac_discriminator_value_t *p_discriminator_val, void *data) {

    if (write_operation == 0) {
        *p_discriminator_val = 0;
    }
    return 0;
}
/*****************************************************************************/
#define GENERAL_TYPE_CHECK_BEGIN(_type_) do { \
		/*need to use the type, in the opposite case we'll not get type definintion in DWARF*/ \
		_type_ * _ptr = NULL; \
		char * _type_name = #_type_; \
		struct metac_type *type = &METAC_TYPE_NAME(_type_); \
		struct metac_type *actual_type = metac_type_actual_type(type);

#define GENERAL_TYPE_CHECK_BYTE_SIZE() do { \
			fail_unless(metac_type_byte_size(type) == sizeof(*_ptr), "metac_type_byte_size returned incorrect value for %s", _type_name); \
		}while(0)
#define GENERAL_TYPE_CHECK_NAME() do { \
			char * type_name = metac_type_name(type); \
			/*workaround for long int and short int - get only first word*/ \
			char * type_name_copy = strdup((type_name!=NULL)?type_name:""), \
				 * tmp = strchr(type_name_copy, ' '); \
			if (tmp != NULL)*tmp = '\0';  \
			fail_unless(strcmp(type_name_copy, _type_name) == 0, "type name returned '%s' instead of '%s'", metac_type_name(type), _type_name);\
			free(type_name_copy);\
		}while(0)
#define GENERAL_TYPE_CHECK_ACCESS_BY_NAME() do { \
			struct metac_type *type_by_name = metac_type_by_name(&METAC_TYPES_ARRAY, _type_name);\
			fail_unless(type_by_name == type, "metac_type_by_name returned incorrect value %p", type_by_name);\
		}while(0)
#define GENERAL_TYPE_CHECK_KIND(_kind_) do { \
			fail_unless(type->kind == _kind_, "ID: must be " #_kind_ ", but it's 0x%x", (int)type->kind); \
		}while(0)
#define GENERAL_TYPE_CHECK_NOT_TYPEDEF_KIND(_kind_) do { \
			fail_unless(actual_type->kind == _kind_, "NOT_TYPEDEF_ID: must be " #_kind_ ", but it's 0x%x", (int)actual_type->kind); \
		}while(0)
void _check_annotations(struct metac_type *type,
        metac_type_annotation_t *override_annotations,
        char **pp_annotation_keys, metac_type_annotation_t **expected_results) {
    int i = 0;
    mark_point();
    while (pp_annotation_keys[i] != NULL) {
        const metac_type_annotation_t *p_annotation = metac_type_annotation(
                type, pp_annotation_keys[i], override_annotations);
        fail_unless(p_annotation != NULL,
                "metac_type_annotation returned NULL unexpectidly for key %s",
                pp_annotation_keys[i]);
        fail_unless(strcmp(p_annotation->key, pp_annotation_keys[i]) == 0,
                "got incorrect annotation");
        if (expected_results[i] != NULL) {
            fail_unless(p_annotation == expected_results[i],
                    "annotation found unexpected result %p instead of %p, key %s",
                    p_annotation, expected_results[i], pp_annotation_keys[i]);
        }
        ++i;
    }
}
#define GENERAL_TYPE_CHECK_ANNOTATIONS(_params_...) do { \
			_check_annotations(type, _params_); \
		}while(0)
#define _GENERAL_TYPE_SKIP_ANNOTATIONS NULL, (char*[]){NULL}, (metac_type_annotation_t*[]){NULL}
#define GENERAL_TYPE_CHECK_PRECOMILED() \
do {\
		/* TBD: metac_precompiled_type_t * p_precompiled_type = metac_precompile_type(type, NULL); \
		fail_unless(p_precompiled_type != NULL, "metac_precompile_type failed for %s", _type_name); \
		if (p_precompiled_type != NULL) { \
			metac_dump_precompiled_type(p_precompiled_type); \
			metac_free_precompiled_type(&p_precompiled_type); \
		}*/ \
	}while(0)
//#define GENERAL_TYPE_CHECK_PRECOMILED() \
//do {\
//		metac_precompiled_type_t * precompiled_type = metac_precompile_type(type); \
//		fail_unless(precompiled_type != NULL || type->id == METAC_KND_subprogram, "metac_precompile_type failed for %s", #_type_); \
//		if (precompiled_type != NULL) { \
//			metac_dump_precompiled_type(precompiled_type); \
//			_type_ x; \
//			_type_ *copy; \
//			memset(&x, 0, sizeof(x)); \
//			fail_unless(metac_visit(&x, sizeof(x), precompiled_type, 1, NULL, 0, &_basic_visitor) == 0, "metac_visit failed"); \
//			fail_unless(metac_visit2(&x, precompiled_type, 1, &_basic_visitor2) == 0, "metac_visit2 failed"); \
//			fail_unless(metac_copy(&x, sizeof(x), precompiled_type, 1, (void**)&copy) == 0, "metac_copy failed"); \
//			fail_unless(metac_delete(copy, sizeof(x), precompiled_type, 1) == 0, "metac_delete failed"); \
//			metac_free_precompiled_type(&precompiled_type); \
//		} \
//	}while(0)
//#define GENERAL_TYPE_CHECK_JSON_UNPACK_PACK() \
//do {\
//		metac_precompiled_type_t * precompiled_type = metac_precompile_type(type); \
//		fail_unless(precompiled_type != NULL || type->id == METAC_KND_subprogram, "metac_precompile_type failed for %s", #_type_); \
//		if (precompiled_type != NULL) { \
//			_type_ x; \
//			_type_ * p_x = NULL; \
//			metac_byte_size_t size = 0; \
//			metac_count_t elements_count = 0; \
//			json_object * p_json = NULL;\
//			memset(&x, 0, sizeof(x)); \
//			fail_unless(metac_unpack_to_json(precompiled_type, &x, sizeof(x), 1, &p_json) == 0, "metac_unpack_to_json failed"); \
//			fail_unless(p_json != NULL, "metac_pack2json hasn't failed, but didn't return the object"); \
//			printf("json:\n %s\n", json_object_to_json_string(p_json)); \
//			fail_unless(metac_pack_from_json(precompiled_type, p_json, (void **)&p_x, &size, &elements_count) == 0, "metac_pack_from_json failed"); \
//			json_object_put(p_json); \
//			p_json = NULL; \
//			fail_unless(size == sizeof(x), "metac_unpack_to_json returned unexpected size"); \
//			fail_unless(elements_count == 1, "metac_unpack_to_json returned unexpected size"); \
//			fail_unless(metac_unpack_to_json(precompiled_type, p_x, sizeof(x), 1, &p_json) == 0, "metac_pack2json failed"); \
//			fail_unless(p_json != NULL, "metac_unpack_to_json hasn't failed, but didn't return the object"); \
//			printf("json_from_packed_obj:\n %s\n", json_object_to_json_string(p_json)); \
//			json_object_put(p_json); \
//			p_json = NULL; \
//			if (metac_equal(&x, p_x, sizeof(x), precompiled_type, 1) != 1) { \
//				DUMP_MEM("Original        : ", &x, sizeof(x)); \
//				DUMP_MEM("Packed from json: ", p_x, sizeof(x)); \
//			}\
//			fail_unless(metac_equal(&x, p_x, sizeof(x), precompiled_type, 1) == 1, "object created by metac_pack_from_json isn't equal with the original"); \
//			fail_unless(metac_delete(p_x, sizeof(x), precompiled_type, 1) == 0, "metac_delete unexpectedly failed"); \
//			metac_free_precompiled_type(&precompiled_type); \
//		} \
//	}while(0)
#define ENUM_TYPE_CHECK_NAME(_expected_name_, _expected_name_skip_typedef_) do {\
		char * expected_name_skip_typedef = _expected_name_skip_typedef_ /*can be NULL for anonymous enums*/; \
		char * type_name = metac_type_name(type); \
		char * type_name_skip_typedef = metac_type_name(actual_type); \
		fail_unless(strcmp(type_name, _expected_name_) == 0, \
				"check_name: %s instead of %s", type_name, _expected_name_); \
		if (expected_name_skip_typedef != NULL) \
			fail_unless(strcmp(type_name_skip_typedef, expected_name_skip_typedef) == 0, \
					"check_name_skip_typedef:  %s instead of %s", type_name_skip_typedef, expected_name_skip_typedef); \
		else \
			fail_unless(type_name_skip_typedef == NULL, \
					"check_name_skip_typedef: Got non-NULL string instead of NULL"); \
		}while(0)
#define ENUM_TYPE_CHECK_VALS(_expected_vals_...) do {\
			static struct metac_type_enumerator_info expected_values[] = _expected_vals_; \
			int i = 0; \
			fail_unless(actual_type->enumeration_type_info.byte_size == sizeof(*_ptr), \
					"enum byte_size %d donsn't match sizeof value %d", \
					type->enumeration_type_info.byte_size, \
					sizeof(*_ptr)); \
			while (expected_values[i].name != NULL) { \
				metac_const_value_t const_value = 0; \
				metac_name_t res_name = metac_type_enumeration_type_get_name(actual_type, expected_values[i].const_value); \
				fail_unless(metac_type_enumeration_type_get_value(actual_type, expected_values[i].name, &const_value) == 0, \
						"metac_type_enumeration_type_get_value failed for %s", expected_values[i].name); \
				fail_unless(expected_values[i].const_value == const_value, "got incorrect value");\
				fail_unless(strcmp(res_name, expected_values[i].name) == 0, "got incorrect name");\
				++i;\
			}\
			fail_unless(i == actual_type->enumeration_type_info.enumerators_count, "incorrect enumerators_count"); \
		}while(0)
#define ARRAY_TYPE_CHECK_VALS(_element_type_name_, _expected_is_flexible_) do { \
			char * element_type_name = _element_type_name_; \
			int is_flexible = _expected_is_flexible_; \
			fail_unless(is_flexible == actual_type->array_type_info.is_flexible, "is_flexible flag doesn't match"); \
			fail_unless(strcmp(metac_type_name(actual_type->array_type_info.type), element_type_name) == 0, \
				"is_flexible flag doesn't match: %s, expected %s", \
				metac_type_name(actual_type->array_type_info.type),\
				element_type_name); \
		}while(0)
struct _subrange_info {
    int res;
    metac_count_t count;
};
#define ARRAY_TYPE_CHECK_SUBRANGES(_expected_subranges_count, _expected_subranges_...) do { \
			metac_num_t i; \
			metac_num_t subranges_count = _expected_subranges_count; \
			static struct _subrange_info expected_subranges[] = _expected_subranges_; \
			fail_unless(subranges_count == actual_type->array_type_info.subranges_count, \
				"Subranges count doesn't match: %d, expected %d", actual_type->array_type_info.subranges_count, subranges_count); \
			for (i = 0; i < subranges_count; i++) {\
				metac_count_t count; \
				int res = metac_type_array_subrange_count(actual_type, i, &count);\
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
#define ARRAY_TYPE_CHECK_LOCATION(_obj_indx_, _should_warn_, _n_indx_, indx...) do { \
			metac_num_t subranges_count = _n_indx_;\
			int should_warn = _should_warn_; \
			metac_num_t subranges[] = indx; \
			metac_data_member_location_t data_member_location = 0;\
			int call_res = metac_type_array_member_location(actual_type, \
					subranges_count, subranges, &data_member_location); \
			fail_unless(call_res >= 0, "metac_type_array_member_location failed"); \
			fail_unless(should_warn == call_res, "warning doesn't work as expected"); \
			fail_unless(((int)_array_delta(obj, _obj_indx_)) == (int)data_member_location, "_delta is different: %x and got %x", \
					(int)_array_delta(obj, _obj_indx_), (int)data_member_location); \
		}while(0)
#define STRUCT_UNION_TYPE_CHECK_BYTESIZE(_type_info_) do { \
			metac_byte_size_t byte_size = sizeof(obj); \
			fail_unless(byte_size == actual_type->_type_info_.byte_size, "byte_size doesn't match"); \
		}while(0)
struct _member_info {
    metac_name_t name;
    metac_data_member_location_t location;
    int flag;
};
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
#define STRUCT_UNION_TYPE_CHECK_MEMBER(_member_name_) \
	{.flag = 1, .name = #_member_name_ , .location = _struct_union_delta(obj, . _member_name_), }
#define STRUCT_UNION_TYPE_CHECK_ANON_MEMBER(_first_member_name_) \
	{.flag = 1, .name = METAC_ANON_MEMBER_NAME, .location = _struct_union_delta(obj, . _first_member_name_), }
#define STRUCT_UNION_TYPE_CHECK_MEMBER_BITFIELD(_member_name_) \
	{.flag = 2, .name = #_member_name_, .location = _struct_union_bit_location(_member_name_), }
#define STRUCT_UNION_TYPE_CHECK_MEMBERS(_type_info_, _expected_members_count, _expected_members_...) do { \
			metac_num_t i; \
			metac_num_t members_count = _expected_members_count; \
			struct _member_info expected_members[] = _expected_members_; \
			fail_unless(members_count == actual_type->_type_info_.members_count, "members_count doesn't match"); \
			for (i = 0; i < members_count; i++) {\
				fail_unless(strcmp(actual_type->_type_info_.members[i].name, expected_members[i].name) == 0, "name mismatch");\
				fail_unless(actual_type->_type_info_.members[i].type != NULL, "type is NULL");\
				switch(expected_members[i].flag) {\
				case 1: \
				fail_unless(actual_type->_type_info_.members[i].data_member_location == expected_members[i].location, "incorrect location"); \
				break; \
				case 2: \
				fail_unless(actual_type->_type_info_.members[i].p_bit_size != NULL, "bit_size must be set"); \
				fail_unless(expected_members[i].location >= actual_type->_type_info_.members[i].data_member_location && \
						expected_members[i].location < actual_type->_type_info_.members[i].data_member_location + metac_type_byte_size(actual_type->_type_info_.members[i].type), \
					"incorrect bit location: member %s, got %x, expected in range [%x, %x)", \
					expected_members[i].name,\
					actual_type->_type_info_.members[i].data_member_location, \
					expected_members[i].location, \
					expected_members[i].location < actual_type->_type_info_.members[i].data_member_location + metac_type_byte_size(actual_type->_type_info_.members[i].type)); \
				break; \
				} \
			} \
		}while(0)
#define FUNCTION_CHECK_GLOBALLY_ACCESSIBLE do { \
			struct metac_object * p_object; \
			mark_point(); \
			p_object = metac_object_by_name(&METAC_OBJECTS_ARRAY, _type_name); \
			fail_unless(p_object != NULL, "metac_object_by_name returned incorrect value %p", p_object);\
			fail_unless(p_object->type == actual_type, "p_object_info.type must be == type");\
		}while(0)
#define FUNCTION_CHECK_RETURN_TYPE(_return_type_name_) do { \
			char* return_type_name = _return_type_name_; \
			if (return_type_name == NULL) {\
				fail_unless(actual_type->subprogram_info.type == NULL, "return type doesn't correlate with expected"); \
			} else { \
				fail_unless(metac_type_name(actual_type->subprogram_info.type) != NULL, "can't get name - (PS: will not work for arrays)"); \
				fail_unless(strcmp(return_type_name, metac_type_name(actual_type->subprogram_info.type)) == 0, "type name doesn't match"); \
			} \
		}while(0)
#define FUNCTION_CHECK_PARAMS(_bug_with_unspecified_parameters_, _parameters_count_, _expected_parameters_...) do { \
			metac_num_t i; \
			int bug_with_unspecified_parameters = _bug_with_unspecified_parameters_; \
			metac_num_t parameters_count = _parameters_count_; \
			struct metac_type_subprogram_parameter expected_parameters[] = _expected_parameters_; \
			fail_unless(parameters_count == actual_type->subprogram_info.parameters_count + bug_with_unspecified_parameters, \
				"parameters_count doesn't match"); \
			for (i = 0; i < actual_type->subprogram_info.parameters_count; i++) { \
				fail_unless(actual_type->subprogram_info.parameters[i].unspecified_parameters == expected_parameters[i].unspecified_parameters, \
					"unspecified_parameters parameter mismatch"); \
				if (expected_parameters[i].unspecified_parameters == 0) { \
					fail_unless(strcmp(actual_type->subprogram_info.parameters[i].name, expected_parameters[i].name) == 0, "name mismatch %s vs %s", \
						actual_type->subprogram_info.parameters[i].name, expected_parameters[i].name);\
				} \
			} \
		}while(0)
#define GENERAL_TYPE_CHECK_END \
	}while(0)
/*****************************************************************************/
#define BASE_TYPE_CHECK(_type_, _id_, _n_td_id_, _annotation_params_...) \
	GENERAL_TYPE_CHECK_BEGIN(_type_) { \
		mark_point(); \
		GENERAL_TYPE_CHECK_BYTE_SIZE(); \
		GENERAL_TYPE_CHECK_NAME(); \
		GENERAL_TYPE_CHECK_ACCESS_BY_NAME(); \
		GENERAL_TYPE_CHECK_KIND(_id_); \
		GENERAL_TYPE_CHECK_NOT_TYPEDEF_KIND(_n_td_id_); \
		GENERAL_TYPE_CHECK_ANNOTATIONS(_annotation_params_); \
		GENERAL_TYPE_CHECK_PRECOMILED(); \
	}GENERAL_TYPE_CHECK_END
/*****************************************************************************/
#define POINTER_TYPE_CHECK BASE_TYPE_CHECK
/*****************************************************************************/
#define ENUM_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_, _expected_name_, _expected_name_skip_typedef_) \
	GENERAL_TYPE_CHECK_BEGIN(_type_) { \
		mark_point(); \
		GENERAL_TYPE_CHECK_BYTE_SIZE(); \
		GENERAL_TYPE_CHECK_NAME(); \
		GENERAL_TYPE_CHECK_ACCESS_BY_NAME(); \
		GENERAL_TYPE_CHECK_KIND(_id_); \
		GENERAL_TYPE_CHECK_NOT_TYPEDEF_KIND(_n_td_id_); \
		GENERAL_TYPE_CHECK_PRECOMILED(); \
		ENUM_TYPE_CHECK_NAME(_expected_name_, _expected_name_skip_typedef_);
#define ENUM_TYPE_CHECK_ANNOTATIONS GENERAL_TYPE_CHECK_ANNOTATIONS
#define ENUM_TYPE_CHECK_END }GENERAL_TYPE_CHECK_END
/*****************************************************************************/
#define ARRAY_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_, _init_...) \
	GENERAL_TYPE_CHECK_BEGIN(_type_) { \
		mark_point(); \
		static _type_ obj = _init_; \
		GENERAL_TYPE_CHECK_BYTE_SIZE(); \
		GENERAL_TYPE_CHECK_NAME(); \
		GENERAL_TYPE_CHECK_ACCESS_BY_NAME(); \
		GENERAL_TYPE_CHECK_KIND(_id_); \
		GENERAL_TYPE_CHECK_NOT_TYPEDEF_KIND(_n_td_id_); \
		GENERAL_TYPE_CHECK_PRECOMILED();
#define ARRAY_TYPE_CHECK_ANNOTATIONS GENERAL_TYPE_CHECK_ANNOTATIONS
#define ARRAY_TYPE_CHECK_END }GENERAL_TYPE_CHECK_END
/*****************************************************************************/
#define STRUCT_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_, _init_...) \
	GENERAL_TYPE_CHECK_BEGIN(_type_) { \
		mark_point(); \
		static _type_ obj = _init_; \
		GENERAL_TYPE_CHECK_BYTE_SIZE(); \
		GENERAL_TYPE_CHECK_NAME(); \
		GENERAL_TYPE_CHECK_ACCESS_BY_NAME(); \
		GENERAL_TYPE_CHECK_KIND(_id_); \
		GENERAL_TYPE_CHECK_NOT_TYPEDEF_KIND(_n_td_id_); \
		GENERAL_TYPE_CHECK_PRECOMILED();
#define STRUCT_TYPE_CHECK_BYTESIZE \
	STRUCT_UNION_TYPE_CHECK_BYTESIZE(structure_type_info)
#define STRUCT_TYPE_CHECK_ANNOTATIONS GENERAL_TYPE_CHECK_ANNOTATIONS
#define STRUCT_TYPE_CHECK_MEMBER STRUCT_UNION_TYPE_CHECK_MEMBER
#define STRUCT_TYPE_CHECK_ANON_MEMBER STRUCT_UNION_TYPE_CHECK_ANON_MEMBER
#define STRUCT_TYPE_CHECK_MEMBER_BITFIELD STRUCT_UNION_TYPE_CHECK_MEMBER_BITFIELD
#define STRUCT_TYPE_CHECK_MEMBERS(_expected_members_count, _expected_members_...) \
		STRUCT_UNION_TYPE_CHECK_MEMBERS(structure_type_info, _expected_members_count, _expected_members_)
#define STRUCT_TYPE_CHECK_END }GENERAL_TYPE_CHECK_END
/*****************************************************************************/
#define UNION_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_, _init_...) \
	GENERAL_TYPE_CHECK_BEGIN(_type_) { \
		mark_point(); \
		static _type_ obj = _init_; \
		GENERAL_TYPE_CHECK_BYTE_SIZE(); \
		GENERAL_TYPE_CHECK_NAME(); \
		GENERAL_TYPE_CHECK_ACCESS_BY_NAME(); \
		GENERAL_TYPE_CHECK_KIND(_id_); \
		GENERAL_TYPE_CHECK_NOT_TYPEDEF_KIND(_n_td_id_); \
		GENERAL_TYPE_CHECK_PRECOMILED();
#define UNION_TYPE_CHECK_BYTESIZE \
	STRUCT_UNION_TYPE_CHECK_BYTESIZE(union_type_info)
#define UNION_TYPE_CHECK_ANNOTATIONS GENERAL_TYPE_CHECK_ANNOTATIONS
#define UNION_TYPE_CHECK_MEMBER STRUCT_UNION_TYPE_CHECK_MEMBER
#define UNION_TYPE_CHECK_ANON_MEMBER STRUCT_UNION_TYPE_CHECK_ANON_MEMBER
#define UNION_TYPE_CHECK_MEMBER_BITFIELD STRUCT_UNION_TYPE_CHECK_MEMBER_BITFIELD
#define UNION_TYPE_CHECK_MEMBERS(_expected_members_count, _expected_members_...) \
		STRUCT_UNION_TYPE_CHECK_MEMBERS(union_type_info, _expected_members_count, _expected_members_)
#define UNION_TYPE_CHECK_END }GENERAL_TYPE_CHECK_END
/*****************************************************************************/
#define FUNCTION_TYPE_CHECK_BEGIN(_type_, _id_, _n_td_id_)  do { \
	char * _type_name = #_type_; \
	struct metac_type *type = &METAC_TYPE_NAME(_type_); \
	struct metac_type *actual_type = metac_type_actual_type(type); \
	{ \
		mark_point(); \
		GENERAL_TYPE_CHECK_NAME(); \
		GENERAL_TYPE_CHECK_ACCESS_BY_NAME(); \
		GENERAL_TYPE_CHECK_KIND(_id_); \
		GENERAL_TYPE_CHECK_NOT_TYPEDEF_KIND(_n_td_id_);
#define FUNCTION_TYPE_CHECK_END \
	}GENERAL_TYPE_CHECK_END
/*****************************************************************************/
/*****************************************************************************/
METAC_DECLARE_EXTERN_TYPES_ARRAY;
METAC_DECLARE_EXTERN_OBJECTS_ARRAY;
/*****************************************************************************/
METAC_TYPE_GENERATE_AND_IMPORT(char);
METAC_TYPE_GENERATE_AND_IMPORT(short);
METAC_TYPE_GENERATE_AND_IMPORT(int);
METAC_TYPE_GENERATE_AND_IMPORT(long);
METAC_TYPE_GENERATE_AND_IMPORT(float);
METAC_TYPE_GENERATE_AND_IMPORT(double);

typedef char char_t;
typedef signed char schar_t;
typedef unsigned char uchar_t;

METAC_TYPE_GENERATE_AND_IMPORT(char_t);
METAC_TYPE_GENERATE_AND_IMPORT(schar_t);
METAC_TYPE_GENERATE_AND_IMPORT(uchar_t);

typedef short short_t;
typedef short int shortint_t;
typedef signed short sshort_t;
typedef signed short int sshortint_t;
typedef unsigned short ushort_t;
typedef unsigned short int ushortint_t;

METAC_TYPE_GENERATE_AND_IMPORT(short_t);
METAC_TYPE_GENERATE_AND_IMPORT(shortint_t);
METAC_TYPE_GENERATE_AND_IMPORT(sshort_t);
METAC_TYPE_GENERATE_AND_IMPORT(sshortint_t);
METAC_TYPE_GENERATE_AND_IMPORT(ushort_t);
METAC_TYPE_GENERATE_AND_IMPORT(ushortint_t);

typedef int int_t;
typedef signed signed_t;
typedef signed int sint_t;
typedef unsigned unsigned_t;
typedef unsigned int uint_t;

METAC_TYPE_GENERATE_AND_IMPORT(int_t);
METAC_TYPE_GENERATE_AND_IMPORT(signed_t);
METAC_TYPE_GENERATE_AND_IMPORT(sint_t);
METAC_TYPE_GENERATE_AND_IMPORT(unsigned_t);
METAC_TYPE_GENERATE_AND_IMPORT(uint_t);

typedef long long_t;
typedef long int longint_t;
typedef signed long slong_t;
typedef signed long int slongint_t;
typedef unsigned long ulong_t;
typedef unsigned long int ulongint_t;
METAC_TYPE_GENERATE_AND_IMPORT(long_t);
METAC_TYPE_GENERATE_AND_IMPORT(longint_t);
METAC_TYPE_GENERATE_AND_IMPORT(slong_t);
METAC_TYPE_GENERATE_AND_IMPORT(slongint_t);
METAC_TYPE_GENERATE_AND_IMPORT(ulong_t);
METAC_TYPE_GENERATE_AND_IMPORT(ulongint_t);

typedef long long llong_t;
typedef long long int llongint_t;
typedef signed long long sllong_t;
typedef signed long long int sllongint_t;
typedef unsigned long long ullong_t;
typedef unsigned long long int ullongint_t;
METAC_TYPE_GENERATE_AND_IMPORT(llong_t);
METAC_TYPE_GENERATE_AND_IMPORT(llongint_t);
METAC_TYPE_GENERATE_AND_IMPORT(sllong_t);
METAC_TYPE_GENERATE_AND_IMPORT(sllongint_t);
METAC_TYPE_GENERATE_AND_IMPORT(ullong_t);
METAC_TYPE_GENERATE_AND_IMPORT(ullongint_t);

typedef float float_t;
METAC_TYPE_GENERATE_AND_IMPORT(float_t);
typedef double double_t;
METAC_TYPE_GENERATE_AND_IMPORT(double_t);
typedef long double ldouble_t;
METAC_TYPE_GENERATE_AND_IMPORT(ldouble_t);
typedef float complex floatcomplex_t;
METAC_TYPE_GENERATE_AND_IMPORT(floatcomplex_t);
typedef double complex doublecomplex_t;
METAC_TYPE_GENERATE_AND_IMPORT(doublecomplex_t);
/*commented because in gcc there is an issue with some garbage in memdump*/
//typedef long double complex ldoublecomplex_t;
//METAC_TYPE_GENERATE_AND_IMPORT(ldoublecomplex_t);
START_TEST( base_types_ut) {
    BASE_TYPE_CHECK(char, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(short, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(int, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(long, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(float, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(double, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);

    BASE_TYPE_CHECK(char, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(short, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(int, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(long, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(float, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(double, METAC_KND_base_type, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);

    BASE_TYPE_CHECK(char_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(schar_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(uchar_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);

    BASE_TYPE_CHECK(short_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(shortint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(sshort_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(sshortint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(ushort_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(ushortint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);

    BASE_TYPE_CHECK(int_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(signed_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(sint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(unsigned_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(uint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);

    BASE_TYPE_CHECK(long_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(longint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(slong_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(slongint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(ulong_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(ulongint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);

    BASE_TYPE_CHECK(llong_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(llongint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(sllong_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(sllongint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(ullong_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(ullongint_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);

    BASE_TYPE_CHECK(float_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(double_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(ldouble_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);

    BASE_TYPE_CHECK(floatcomplex_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
    BASE_TYPE_CHECK(doublecomplex_t, METAC_KND_typedef, METAC_KND_base_type,
            _GENERAL_TYPE_SKIP_ANNOTATIONS);
//	BASE_TYPE_CHECK(ldoublecomplex_t, METAC_KND_typedef, METAC_KND_base_type, _GENERAL_TYPE_SKIP_ANNOTATIONS);
}
END_TEST

/*****************************************************************************/
typedef void *voidptr_t;
METAC_TYPE_GENERATE_AND_IMPORT(voidptr_t);
typedef void **voidptrptr_t;
METAC_TYPE_GENERATE_AND_IMPORT(voidptrptr_t);
typedef char *charptr_t;
METAC_TYPE_GENERATE_AND_IMPORT(charptr_t);
struct _incomplete_;
typedef struct _incomplete_ incomplete_t;
typedef incomplete_t *p_incomplete_t;
METAC_TYPE_GENERATE_AND_IMPORT(p_incomplete_t);
typedef const char *cchar_t;
METAC_TYPE_GENERATE_AND_IMPORT(cchar_t);
typedef int_t (*func_ptr_t)(doublecomplex_t *arg);
METAC_TYPE_GENERATE_AND_IMPORT(func_ptr_t);

START_TEST( pointers_ut) {
    POINTER_TYPE_CHECK(voidptr_t, METAC_KND_typedef, METAC_KND_pointer_type, _GENERAL_TYPE_SKIP_ANNOTATIONS)
    ;
    POINTER_TYPE_CHECK(voidptrptr_t, METAC_KND_typedef, METAC_KND_pointer_type, _GENERAL_TYPE_SKIP_ANNOTATIONS)
    ;
    POINTER_TYPE_CHECK(charptr_t, METAC_KND_typedef, METAC_KND_pointer_type, _GENERAL_TYPE_SKIP_ANNOTATIONS)
    ;
    POINTER_TYPE_CHECK(p_incomplete_t, METAC_KND_typedef, METAC_KND_pointer_type, _GENERAL_TYPE_SKIP_ANNOTATIONS)
    ;
    POINTER_TYPE_CHECK(cchar_t, METAC_KND_typedef, METAC_KND_pointer_type, _GENERAL_TYPE_SKIP_ANNOTATIONS)
    ;
    POINTER_TYPE_CHECK(func_ptr_t, METAC_KND_typedef, METAC_KND_pointer_type, _GENERAL_TYPE_SKIP_ANNOTATIONS)
    ;
}
END_TEST

/*****************************************************************************/
typedef enum _enum_ {
    _eZero = 0, _eOne, _eTen = 10, _eEleven, _eTwelve,
} enum_t;
METAC_TYPE_GENERATE_AND_IMPORT(enum_t);
typedef enum {
    aeMinus = -1, aeZero = 0, aeOne = 1, aeTen = 10, aeEleven, aeTwelve,
} anon_enum_t;
METAC_TYPE_GENERATE_AND_IMPORT(anon_enum_t);
typedef enum __attribute__((packed, aligned(16)))_aligned_enum_ {
    al_eZero = 0, al_eOne, al_eTen = 10, al_eEleven, al_eTwelve,
} aligned_enum_t;
METAC_TYPE_GENERATE_AND_IMPORT(aligned_enum_t);

START_TEST( enums_ut) {
    ENUM_TYPE_CHECK_BEGIN(enum_t, METAC_KND_typedef, METAC_KND_enumeration_type, "enum_t", "_enum_")
                {
                    ENUM_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    ENUM_TYPE_CHECK_VALS({ {.name = "_eZero", .const_value = 0},
                            {.name = "_eOne", .const_value = 1},
                            {.name = "_eTen", .const_value = 10},
                            {.name = "_eEleven", .const_value = 11},
                            {.name = "_eTwelve", .const_value = 12}, { .name =
                                    NULL }, });
                }ENUM_TYPE_CHECK_END;
    ENUM_TYPE_CHECK_BEGIN(anon_enum_t, METAC_KND_typedef, METAC_KND_enumeration_type, "anon_enum_t", NULL)
                {
                    ENUM_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    ENUM_TYPE_CHECK_VALS({ {.name = "aeMinus",
                            .const_value = -1}, {.name = "aeZero",
                            .const_value = 0}, {.name = "aeOne",
                            .const_value = 1}, {.name = "aeTen",
                            .const_value = 10}, {.name = "aeEleven",
                            .const_value = 11}, {.name = "aeTwelve",
                            .const_value = 12}, { .name = NULL }, });
                }ENUM_TYPE_CHECK_END;
    ENUM_TYPE_CHECK_BEGIN(aligned_enum_t, METAC_KND_typedef, METAC_KND_enumeration_type, "aligned_enum_t", "_aligned_enum_")
                {
                    ENUM_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    ENUM_TYPE_CHECK_VALS({ {.name = "al_eZero",
                            .const_value = 0}, {.name = "al_eOne",
                            .const_value = 1}, {.name = "al_eTen",
                            .const_value = 10}, {.name = "al_eEleven",
                            .const_value = 11}, {.name = "al_eTwelve",
                            .const_value = 12}, { .name = NULL }, });
                }ENUM_TYPE_CHECK_END;
}
END_TEST
/*****************************************************************************/
typedef char_t char_array_t[0];
METAC_TYPE_GENERATE_AND_IMPORT(char_array_t);
typedef char_t char_array5_t[5];
METAC_TYPE_GENERATE_AND_IMPORT(char_array5_t);
typedef char _2darray_t[2][3];
METAC_TYPE_GENERATE_AND_IMPORT(_2darray_t);
typedef char _3darray_t[5][4][3];
METAC_TYPE_GENERATE_AND_IMPORT(_3darray_t);
typedef char _3darray1_t[5][0][3];
METAC_TYPE_GENERATE_AND_IMPORT(_3darray1_t);

START_TEST( arrays_ut) {
    ARRAY_TYPE_CHECK_BEGIN(char_array_t, METAC_KND_typedef, METAC_KND_array_type, {})
                {
                    ARRAY_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    ARRAY_TYPE_CHECK_VALS("char_t", _BUG_ZERO_LEN_IS_FLEXIBLE_); /*this array looks like a flexible for DWARF*/
                    ARRAY_TYPE_CHECK_SUBRANGES(1, {{_BUG_ZERO_LEN_IS_FLEXIBLE_,
                            0}, });
                }ARRAY_TYPE_CHECK_END;
    ARRAY_TYPE_CHECK_BEGIN(char_array5_t, METAC_KND_typedef, METAC_KND_array_type, {})
                {
                    ARRAY_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    ARRAY_TYPE_CHECK_VALS("char_t", 0);
                    ARRAY_TYPE_CHECK_SUBRANGES(1, {{0, 5}, });
                    ARRAY_TYPE_CHECK_LOCATION([1], 0, 1, {1, });
                }ARRAY_TYPE_CHECK_END;
    ARRAY_TYPE_CHECK_BEGIN(_2darray_t, METAC_KND_typedef, METAC_KND_array_type, {})
                {
                    ARRAY_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    ARRAY_TYPE_CHECK_VALS("char", 0);
                    ARRAY_TYPE_CHECK_SUBRANGES(2, {{0, 2}, {0, 3}, });
                    ARRAY_TYPE_CHECK_LOCATION([0], 0, 1, {0, });
                    ARRAY_TYPE_CHECK_LOCATION([1], 0, 1, {1, });
                    ARRAY_TYPE_CHECK_LOCATION([2], 1, 1, {2, });
                    ARRAY_TYPE_CHECK_LOCATION([0][0], 0, 2, {0, 0, });
                    ARRAY_TYPE_CHECK_LOCATION([0][1], 0, 2, {0, 1, });
                    ARRAY_TYPE_CHECK_LOCATION([0][2], 0, 2, {0, 2, });
                    ARRAY_TYPE_CHECK_LOCATION([0][3], 1, 2, {0, 3, });
                    ARRAY_TYPE_CHECK_LOCATION([1][0], 0, 2, {1, 0, });
                    ARRAY_TYPE_CHECK_LOCATION([1][1], 0, 2, {1, 1, });
                    ARRAY_TYPE_CHECK_LOCATION([1][2], 0, 2, {1, 2, });
                    ARRAY_TYPE_CHECK_LOCATION([1][3], 1, 2, {1, 3, });
                    ARRAY_TYPE_CHECK_LOCATION([2][0], 1, 2, {2, 0, });
                    ARRAY_TYPE_CHECK_LOCATION([2][1], 1, 2, {2, 1, });
                    ARRAY_TYPE_CHECK_LOCATION([2][2], 1, 2, {2, 2, });
                    ARRAY_TYPE_CHECK_LOCATION([3][2], 1, 2, {3, 2, });
                }ARRAY_TYPE_CHECK_END;
    ARRAY_TYPE_CHECK_BEGIN(_3darray_t, METAC_KND_typedef, METAC_KND_array_type, {})
                {
                    ARRAY_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    ARRAY_TYPE_CHECK_VALS("char", 0);
                    ARRAY_TYPE_CHECK_SUBRANGES(3, {{0, 5}, {0, 4}, {0, 3}, });
                    ARRAY_TYPE_CHECK_LOCATION([1][0][0], 0, 3, {1, 0, 0, });
                    ARRAY_TYPE_CHECK_LOCATION([1][0][1], 0, 3, {1, 0, 1, });
                    ARRAY_TYPE_CHECK_LOCATION([1][3][2], 0, 3, {1, 3, 2, });
                }ARRAY_TYPE_CHECK_END;
    ARRAY_TYPE_CHECK_BEGIN(_3darray1_t, METAC_KND_typedef, METAC_KND_array_type, {})
                {
                    ARRAY_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    ARRAY_TYPE_CHECK_VALS("char", 0); /*this array looks like a flexible for DWARF*/
                    ARRAY_TYPE_CHECK_SUBRANGES(3, {{0, 5}, {0, 0}, {0, 3}, });
                    ARRAY_TYPE_CHECK_LOCATION([4][2][4], 2, 3, {4, 2, 4, });
                }ARRAY_TYPE_CHECK_END;
}
END_TEST

/*****************************************************************************/
typedef struct _struct_ {
    unsigned int widthValidated;
    unsigned int heightValidated;
} struct_t;
METAC_TYPE_GENERATE_AND_IMPORT(struct_t);
typedef struct _bit_fields_ {
    unsigned int widthValidated :9;
    unsigned int heightValidated :12;
} bit_fields_t;
METAC_TYPE_GENERATE_AND_IMPORT(bit_fields_t);
typedef struct _bit_fields_for_longer_than32_bit {
    unsigned int widthValidated :9;
    unsigned int heightValidated :12;
    int heightValidated1 :30;
    llongint_t heightValidated2 :63;
} bit_fields_for_longer_than32_bit_t;
METAC_TYPE_GENERATE_AND_IMPORT(bit_fields_for_longer_than32_bit_t);
struct _struct_with_flexible_array_and_len {
    int array_len;
    char array[];
};
typedef struct _struct_with_struct_with_flexible_array_and_len {
    int before_struct;
    struct _struct_with_flexible_array_and_len str1;
} struct_with_struct_with_flexible_array_and_len_t;
METAC_TYPE_GENERATE_AND_IMPORT(
        struct_with_struct_with_flexible_array_and_len_t);

METAC_TYPE_ANNOTATION_BEGIN(struct_with_struct_with_flexible_array_and_len_t) METAC_TYPE_ANNOTATION("discrimitator_name", NULL),
METAC_TYPE_ANNOTATION("discrimitator_name1", NULL),
METAC_TYPE_ANNOTATION_END

METAC_TYPE_ANNOTATION_OVERRIDE_BEGIN(override_some_params) METAC_TYPE_ANNOTATION("discrimitator_name", METAC_CALLBACK_DISCRIMINATOR(NULL, NULL)),
METAC_TYPE_ANNOTATION("discrimitator_name2", NULL),
METAC_TYPE_ANNOTATION_END

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
} anon_struct_with_anon_elements;
METAC_TYPE_GENERATE_AND_IMPORT(anon_struct_with_anon_elements);
METAC_TYPE_ANNOTATION_BEGIN(anon_struct_with_anon_elements) METAC_TYPE_ANNOTATION("ptr[]", METAC_CALLBACK_DISCRIMINATOR(_dummy_discriminator, NULL)),
METAC_TYPE_ANNOTATION_END

struct ___test___ {
    int n;
    char array1[];
};
typedef struct _struct_with_flexible_ND_array_and_len {
    int array_len;
    struct ___test___ x;
    char array1[][3][3]; /*C doesn't allow to create char array[1][]. Also, C doesn't allow to create type with*/
} struct_with_flexible_ND_array_and_len_t;
METAC_TYPE_GENERATE_AND_IMPORT(struct_with_flexible_ND_array_and_len_t);

START_TEST( structs_ut) {
    STRUCT_TYPE_CHECK_BEGIN(struct_t, METAC_KND_typedef, METAC_KND_structure_type, {})
                {
                    STRUCT_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    STRUCT_TYPE_CHECK_BYTESIZE
                    ;
                    STRUCT_TYPE_CHECK_MEMBERS(2,
                            { STRUCT_TYPE_CHECK_MEMBER(widthValidated),
                            STRUCT_TYPE_CHECK_MEMBER(heightValidated), });
                }STRUCT_TYPE_CHECK_END;
    STRUCT_TYPE_CHECK_BEGIN(bit_fields_t, METAC_KND_typedef, METAC_KND_structure_type, {})
                {
                    STRUCT_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    STRUCT_TYPE_CHECK_BYTESIZE
                    ;
                    STRUCT_TYPE_CHECK_MEMBERS(2,
                            { STRUCT_TYPE_CHECK_MEMBER_BITFIELD(widthValidated),
                            STRUCT_TYPE_CHECK_MEMBER_BITFIELD(heightValidated),
                            });
                }STRUCT_TYPE_CHECK_END;
    STRUCT_TYPE_CHECK_BEGIN(bit_fields_for_longer_than32_bit_t, METAC_KND_typedef, METAC_KND_structure_type, {})
                {
                    STRUCT_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    STRUCT_TYPE_CHECK_BYTESIZE
                    ;
                    STRUCT_TYPE_CHECK_MEMBERS(4,
                            { STRUCT_TYPE_CHECK_MEMBER_BITFIELD(widthValidated),
                            STRUCT_TYPE_CHECK_MEMBER_BITFIELD(heightValidated),
                            STRUCT_TYPE_CHECK_MEMBER_BITFIELD(heightValidated1),
                            STRUCT_TYPE_CHECK_MEMBER_BITFIELD(heightValidated2),
                            });
                }STRUCT_TYPE_CHECK_END;
    STRUCT_TYPE_CHECK_BEGIN(struct_with_struct_with_flexible_array_and_len_t, METAC_KND_typedef, METAC_KND_structure_type, {})
                {
                    STRUCT_TYPE_CHECK_ANNOTATIONS(override_some_params, (char*[]) {
                                "discrimitator_name",
                                "discrimitator_name2",
                                NULL
                            }, (metac_type_annotation_t*[]) {
                                &override_some_params[0],
                                &override_some_params[1],
                                NULL
                            })
                    ;
                    STRUCT_TYPE_CHECK_BYTESIZE
                    ;
                    STRUCT_TYPE_CHECK_MEMBERS(2,
                            { STRUCT_TYPE_CHECK_MEMBER(before_struct),
                            STRUCT_TYPE_CHECK_MEMBER(str1), });
                }STRUCT_TYPE_CHECK_END;
    STRUCT_TYPE_CHECK_BEGIN(anon_struct_with_anon_elements, METAC_KND_typedef, METAC_KND_structure_type, {})
                {
                    STRUCT_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    STRUCT_TYPE_CHECK_BYTESIZE
                    ;
                    STRUCT_TYPE_CHECK_MEMBERS(4,
                            { STRUCT_TYPE_CHECK_ANON_MEMBER(a),
                            STRUCT_TYPE_CHECK_ANON_MEMBER(c),
                            STRUCT_TYPE_CHECK_ANON_MEMBER(x),
                            STRUCT_TYPE_CHECK_MEMBER(e), });
                }STRUCT_TYPE_CHECK_END;
    STRUCT_TYPE_CHECK_BEGIN(struct_with_flexible_ND_array_and_len_t, METAC_KND_typedef, METAC_KND_structure_type, {})
                {
                    STRUCT_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    STRUCT_TYPE_CHECK_BYTESIZE
                    ;
//		STRUCT_TYPE_CHECK_MEMBERS(4, {
//			STRUCT_TYPE_CHECK_ANON_MEMBER(a),
//			STRUCT_TYPE_CHECK_ANON_MEMBER(c),
//			STRUCT_TYPE_CHECK_ANON_MEMBER(x),
//			STRUCT_TYPE_CHECK_MEMBER(e),
//		});
                }STRUCT_TYPE_CHECK_END;
    struct_with_flexible_ND_array_and_len_t x[10];
}
END_TEST
/*****************************************************************************/
typedef union _union_ {
    int d;
    char f;
} union_t;
METAC_TYPE_GENERATE_AND_IMPORT(union_t);
METAC_TYPE_ANNOTATION_BEGIN(union_t) METAC_TYPE_ANNOTATION("ptr[]", METAC_CALLBACK_DISCRIMINATOR(_dummy_discriminator, NULL)),
METAC_TYPE_ANNOTATION_END

typedef union _union_anon_ {
    struct {
        char a;
        int b;
    };
    long c;
    struct {
        int d;
        char f;
    };
} union_anon_t;
METAC_TYPE_GENERATE_AND_IMPORT(union_anon_t);
METAC_TYPE_ANNOTATION_BEGIN(union_anon_t) METAC_TYPE_ANNOTATION("ptr[]", METAC_CALLBACK_DISCRIMINATOR(_dummy_discriminator, NULL)),
METAC_TYPE_ANNOTATION_END

START_TEST( unions_ut) {
    UNION_TYPE_CHECK_BEGIN(union_t, METAC_KND_typedef, METAC_KND_union_type, {})
                {
                    UNION_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    UNION_TYPE_CHECK_BYTESIZE
                    ;
                    UNION_TYPE_CHECK_MEMBERS(2, { UNION_TYPE_CHECK_MEMBER(d),
                            UNION_TYPE_CHECK_MEMBER(f), });
                }UNION_TYPE_CHECK_END;
    UNION_TYPE_CHECK_BEGIN(union_anon_t, METAC_KND_typedef, METAC_KND_union_type, {})
                {
                    UNION_TYPE_CHECK_ANNOTATIONS(_GENERAL_TYPE_SKIP_ANNOTATIONS)
                    ;
                    UNION_TYPE_CHECK_BYTESIZE
                    ;
                    UNION_TYPE_CHECK_MEMBERS(3,
                            { UNION_TYPE_CHECK_ANON_MEMBER(a),
                            UNION_TYPE_CHECK_MEMBER(c),
                            UNION_TYPE_CHECK_ANON_MEMBER(d), });
                }UNION_TYPE_CHECK_END;
}
END_TEST
/*****************************************************************************/
typedef bit_fields_t *p_bit_fields_t;
int_t func_t(p_bit_fields_t arg) {
    if (arg)
        return 1;
    return 0;
}
METAC_FUNCTION(func_t);
void func_printf(cchar_t format, ...) {
    return;
}
METAC_FUNCTION(func_printf);
void func_vprintf(const char *format, va_list ap) {
    return;
}
METAC_FUNCTION(func_vprintf);

START_TEST( funtions_ut) {
    FUNCTION_TYPE_CHECK_BEGIN(func_t, METAC_KND_subprogram, METAC_KND_subprogram)
                {
                    FUNCTION_CHECK_GLOBALLY_ACCESSIBLE
                    ;
                    FUNCTION_CHECK_RETURN_TYPE("int_t");
                    FUNCTION_CHECK_PARAMS(_BUG_NO_USPECIFIED_PARAMETER_, 1,
                            { {.name = "arg", }, });
                }FUNCTION_TYPE_CHECK_END;
    FUNCTION_TYPE_CHECK_BEGIN(func_printf, METAC_KND_subprogram, METAC_KND_subprogram)
                {
                    FUNCTION_CHECK_GLOBALLY_ACCESSIBLE
                    ;
                    FUNCTION_CHECK_RETURN_TYPE(NULL);
                    FUNCTION_CHECK_PARAMS(_BUG_NO_USPECIFIED_PARAMETER_, 2,
                            { {.name = "format", },
                            {.unspecified_parameters = 1, }, });
                }FUNCTION_TYPE_CHECK_END;
    FUNCTION_TYPE_CHECK_BEGIN(func_vprintf, METAC_KND_subprogram, METAC_KND_subprogram)
                {
                    FUNCTION_CHECK_GLOBALLY_ACCESSIBLE
                    ;
                    FUNCTION_CHECK_RETURN_TYPE(NULL);
                    FUNCTION_CHECK_PARAMS(_BUG_NO_USPECIFIED_PARAMETER_, 2,
                            { {.name = "format", }, {.name = "ap", }, });
                }FUNCTION_TYPE_CHECK_END;
}
END_TEST
/*****************************************************************************/
START_TEST( metac_array_symbols) {
    mark_point();
    mark_point();
    void *handle = dlopen(NULL, RTLD_NOW);
    fail_unless(handle != NULL, "dlopen failed");
    void *types_array = dlsym(handle, METAC_TYPES_ARRAY_SYMBOL);
    void *objects_array = dlsym(handle, METAC_OBJECTS_ARRAY_SYMBOL);
    dlclose(handle);
    fail_unless(types_array == &METAC_TYPES_ARRAY, "can't find correct %s: %p",
    METAC_TYPES_ARRAY_SYMBOL, types_array);
    fail_unless(objects_array == &METAC_OBJECTS_ARRAY,
            "can't find correct %s: %p", METAC_OBJECTS_ARRAY_SYMBOL,
            objects_array);
}
END_TEST
/*****************************************************************************/
static int _metac_type_t_discriminator_funtion(char *annotation_key,
        int write_operation, /* 0 - if need to store date to p_discriminator_val, 1 - vice-versa*/
        void *ptr, metac_type_t *type, /*pointer to memory region and its type */
        metac_discriminator_value_t *p_discriminator_val,
        void *specification_context) {

    char *key = (char*) specification_context;
    msg_stddbg(
            "callback: _metac_type_t_discriminator_funtion write_operation %d, key %s\n",
            write_operation, key);

    if (strcmp(key, "<ptr>.<anon0>") == 0) {
        metac_type_t *metac_type_obj = (metac_type_t*) ptr;
        if (write_operation == 0) {
            switch (metac_type_obj->kind) {
            case METAC_KND_typedef:
                *p_discriminator_val = 0;
                return 0;
            case METAC_KND_const_type:
                *p_discriminator_val = 1;
                return 0;
            case METAC_KND_base_type:
                *p_discriminator_val = 2;
                return 0;
            case METAC_KND_pointer_type:
                *p_discriminator_val = 3;
                return 0;
            case METAC_KND_enumeration_type:
                *p_discriminator_val = 4;
                return 0;
            case METAC_KND_subprogram:
                *p_discriminator_val = 5;
                return 0;
            case METAC_KND_structure_type:
                *p_discriminator_val = 6;
                return 0;
            case METAC_KND_union_type:
                *p_discriminator_val = 7;
                return 0;
            case METAC_KND_array_type:
                *p_discriminator_val = 8;
                return 0;
            }
            msg_stddbg("callback failed: can't find val for 0x%x\n",
                    (int )metac_type_obj->id);
            *p_discriminator_val = -1; /*there can be other values, but we don't match them with any union value*/
            return 0; /*-1;*/
        } else {
            switch (*p_discriminator_val) {
            case 0:
                metac_type_obj->kind = METAC_KND_typedef;
                return 0;
            case 1:
                metac_type_obj->kind = METAC_KND_const_type;
                return 0;
            case 2:
                metac_type_obj->kind = METAC_KND_base_type;
                return 0;
            case 3:
                metac_type_obj->kind = METAC_KND_pointer_type;
                return 0;
            case 4:
                metac_type_obj->kind = METAC_KND_enumeration_type;
                return 0;
            case 5:
                metac_type_obj->kind = METAC_KND_subprogram;
                return 0;
            case 6:
                metac_type_obj->kind = METAC_KND_structure_type;
                return 0;
            case 7:
                metac_type_obj->kind = METAC_KND_union_type;
                return 0;
            case 8:
                metac_type_obj->kind = METAC_KND_array_type;
                return 0;
            }
            return -1;
        }
    } /*else if (strcmp(key, "<ptr>.dwarf_info.at.<ptr>.<anon0>") == 0) {
        struct metac_type_at *metac_type_at_obj = (struct metac_type_at*) ptr;
        if (write_operation == 0) {
            switch (metac_type_at_obj->id) {
            case DW_AT_name:
                *p_discriminator_val = 0;
                return 0;
            case DW_AT_type:
                *p_discriminator_val = 1;
                return 0;
            case DW_AT_byte_size:
                *p_discriminator_val = 2;
                return 0;
            case DW_AT_encoding:
                *p_discriminator_val = 3;
                return 0;
            case DW_AT_data_member_location:
                *p_discriminator_val = 4;
                return 0;
            case DW_AT_bit_offset:
                *p_discriminator_val = 5;
                return 0;
            case DW_AT_bit_size:
                *p_discriminator_val = 6;
                return 0;
            case DW_AT_lower_bound:
                *p_discriminator_val = 7;
                return 0;
            case DW_AT_upper_bound:
                *p_discriminator_val = 8;
                return 0;
            case DW_AT_count:
                *p_discriminator_val = 9;
                return 0;
            case DW_AT_const_value:
                *p_discriminator_val = 10;
                return 0;
            case DW_AT_declaration:
                *p_discriminator_val = 11;
                return 0;
            }
            return -1;
        } else {
            switch (*p_discriminator_val) {
            case 0:
                metac_type_at_obj->id = DW_AT_name;
                return 0;
            case 1:
                metac_type_at_obj->id = DW_AT_type;
                return 0;
            case 2:
                metac_type_at_obj->id = DW_AT_byte_size;
                return 0;
            case 3:
                metac_type_at_obj->id = DW_AT_encoding;
                return 0;
            case 4:
                metac_type_at_obj->id = DW_AT_data_member_location;
                return 0;
            case 5:
                metac_type_at_obj->id = DW_AT_bit_offset;
                return 0;
            case 6:
                metac_type_at_obj->id = DW_AT_bit_size;
                return 0;
            case 7:
                metac_type_at_obj->id = DW_AT_lower_bound;
                return 0;
            case 8:
                metac_type_at_obj->id = DW_AT_upper_bound;
                return 0;
            case 9:
                metac_type_at_obj->id = DW_AT_count;
                return 0;
            case 10:
                metac_type_at_obj->id = DW_AT_const_value;
                return 0;
            case 11:
                metac_type_at_obj->id = DW_AT_declaration;
                return 0;
            }
            return -1;
        }
    } */
    return -1;
}

static int _metac_type_t_array_elements_count_funtion(char *annotation_key,
        int write_operation, void *ptr, metac_type_t *type, /*pointer to memory region and its type */
        void *first_element_ptr, metac_type_t *first_element_type,
        metac_num_t *p_subrange0_count, void *array_elements_count_cb_context) {

    int res = -1;
    char *key = (char*) array_elements_count_cb_context;
    metac_type_t *metac_type_obj = (metac_type_t*) ptr;
    msg_stddbg(
            "callback: _metac_type_t_array_elements_count_funtion write_operation %d, key %s\n",
            write_operation, key);

    if (strcmp(key, "<ptr>.<anon0>.enumeration_type_info.enumerators") == 0) {
        *p_subrange0_count =
                metac_type_obj->enumeration_type_info.enumerators_count;
        res = 0;
    } /*else if (strcmp(key, "<ptr>.dwarf_info.at") == 0) {
        *p_subrange0_count = metac_type_obj->dwarf_info.at_num;
        res = 0;
    } else if (strcmp(key, "<ptr>.dwarf_info.child") == 0) {
        *p_subrange0_count = metac_type_obj->dwarf_info.child_num;
        res = 0;
    }*/ else if (strcmp(key, "<ptr>.<anon0>.structure_type_info.members") == 0) {
        *p_subrange0_count = metac_type_obj->structure_type_info.members_count;
        res = 0;
    } else if (strcmp(key, "<ptr>.<anon0>.union_type_info.members") == 0) {
        *p_subrange0_count = metac_type_obj->union_type_info.members_count;
        res = 0;
    } else if (strcmp(key, "<ptr>.<anon0>.array_type_info.subranges") == 0) {
        *p_subrange0_count = metac_type_obj->array_type_info.subranges_count;
        res = 0;
    } else if (strcmp(key, "<ptr>.name") == 0) {
        msg_stddbg("%p ? %p : %s\n", first_element_ptr, metac_type_obj->name,
                metac_type_obj->name);
        return metac_array_elements_1d_with_null(annotation_key,
                write_operation, ptr, type, first_element_ptr,
                first_element_type, p_subrange0_count,
                array_elements_count_cb_context);
    }

    msg_stddbg(
            "callback: _metac_type_t_array_elements_count_funtion write_operation %d, key %s, output to subranges[0].count %d returns %d\n",
            write_operation, key, p_array_info->subranges[0].count, res);
    return res;
}

/*****************************************************************************/
int main(void) {
    printf("bug_with_unspecified_parameters %d\n",
    _BUG_NO_USPECIFIED_PARAMETER_);
    printf("bug_zero_len_is_flexible %d\n", _BUG_ZERO_LEN_IS_FLEXIBLE_);
    return run_suite(
    START_SUITE(type_suite) {
        ADD_CASE(
                START_CASE(type_smoke) {
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
    }END_SUITE);
}

