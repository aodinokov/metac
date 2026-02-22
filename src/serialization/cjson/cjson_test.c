#include "metac/test.h"
#include <stdio.h>
#include <stdlib.h>

// enable test only if needed
#if WITH_CJSON
// including c-file we want to test, because we need to test some static functions
#include "../../entry.c"
#include "../../entry_db.c"
#include "../../entry_cdecl.c"
#include "../../entry_tag.c"
#include "../../iterator.c"
#include "../../hashmap.c"
#include "../../printf_format.c"
#include "../../value.c"
#include "../../value_base_type.c"
#include "../../value_with_args.c"

// testing both these file (not sure why they were integrated together)
#include "value_to_cjson.c"
#include "value_from_cjson.c"

// serialization sanity
// the same sequence as in value_string_test.c
// we can use almost the same tests as with _json_string_ex, but with different representation using:
#define _json_string_ex(p_val, wmode, p_tag_map) ({ \
        char * res = NULL; \
        json = metac_value_to_cjson(p_val, wmode, p_tag_map); \
        if (json != NULL) { \
            res = cJSON_PrintUnformatted(json); \
        } \
        res; \
    })
#define _json_string(p_val) _json_string_ex(p_val, METAC_WMODE_shallow, NULL)

// we need to have a destination p_val to store result of deserialization
// it can be used later (ourside of _check_serialization_) to compare if original p_val and p_val_dest are equal
// There is an assumption that if _check_serialization_ passed, we have got json which can be used for backward converstion.
// This approach can be more efficient to test bi-directional ser/deser positive cases.
// Note: We can't use this for negative deserialization cases
#define __and_back(p_val_dest, p_tag_map, _extra_check...) do { \
        if (p_val_dest != NULL && json != NULL) { \
                fail_unless(metac_value_from_cjson(p_val_dest, json, p_tag_map) == 0, "deserialization failed from %s", _expected_s); \
                do { _extra_check } while(0); \
        } \
    } while(0)

#define _json_string_and_back_ex(p_val_dest, p_val, wmode, p_tag_map, _extra_check...) ({ \
        char * res = _json_string_ex(p_val, wmode, p_tag_map); \
        __and_back(p_val_dest, p_tag_map, _extra_check); \
        res; \
    })
#define _json_string_and_back(p_val_dest, p_val, _extra_check...) _json_string_and_back_ex(p_val_dest, p_val, METAC_WMODE_shallow, NULL, _extra_check)

// main pipeline which has some place to store all intermittent data to run serialization (+deserialization if _and_back macros is used)
#define _check_serialization_(_s_, _s_expected) do { \
        char * _expected_s = _s_expected; \
        struct cJSON* json = NULL; \
        char * _s = _s_; \
        \
        if (_expected_s == NULL) { \
            fail_unless(_s == NULL, "expecting NULL, got not NULL: %s", _s); \
        } else { \
            fail_unless(_s != NULL, "expecting non-NULL %s, got NULL", _expected_s); \
            fail_unless(strcmp(_expected_s, _s) == 0, "expected %s, got %s", _expected_s, _s); \
        } \
        \
        if (_s != NULL) { \
            free(_s); \
        } \
        if (json != NULL) { \
            cJSON_Delete(json); \
        } \
    } while(0)

char test0 = 120;
char test0_dst = -1;
METAC_GSYM_LINK(test0);
METAC_GSYM_LINK(test0_dst);
METAC_START_TEST(test0_sanity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test0);
    fail_unless(p_val != NULL, "wasn't able to get value");
    metac_value_t * p_val_dst = METAC_VALUE_FROM_LINK(test0_dst);
    fail_unless(p_val_dst != NULL, "p_val_dst is NULL");

    _check_serialization_(
        _json_string_and_back(p_val_dst, p_val, 
            fail_unless(test0_dst == test0, "exected test0_dst %d to be equial to test0 %d", (int)test0_dst, (int)test0);
        ), 
        "120"
    );

    metac_value_delete(p_val_dst);
    metac_value_delete(p_val);
}END_TEST

int test1 = 31;
int test1_dst = -1;
METAC_GSYM_LINK(test1);
METAC_GSYM_LINK(test1_dst);
METAC_START_TEST(test1_sanity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test1);
    fail_unless(p_val != NULL, "wasn't able to get value");
    metac_value_t * p_val_dst = METAC_VALUE_FROM_LINK(test1_dst);
    fail_unless(p_val_dst != NULL, "p_val_dst is NULL");

    _check_serialization_(
        _json_string_and_back(p_val_dst, p_val,
            fail_unless(test1_dst == test1, "exected test1_dst %d to be equial to test1 %d", test1_dst, test1);
        ), 
        "31"
    );

    metac_value_delete(p_val_dst);
    metac_value_delete(p_val);
}END_TEST

enum {
    _x_TstA = -1,
    _x_TstB = 1,
    _x_TstX = 7,
} test2 = -1, test2_dst = 1;
METAC_GSYM_LINK(test2);
METAC_GSYM_LINK(test2_dst);
METAC_START_TEST(test2_sanity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test2);
    fail_unless(p_val != NULL, "wasn't able to get value");
    metac_value_t * p_val_dst = METAC_VALUE_FROM_LINK(test2_dst);
    fail_unless(p_val_dst != NULL, "p_val_dst is NULL");


    _check_serialization_(
        _json_string_and_back(p_val_dst, p_val,
            fail_unless(test2_dst == test2, "exected test2_dst %d to be equial to test2 %d", (int)test2_dst, (int)test2);

        ), 
        "\"_x_TstA\""
    );

    metac_value_delete(p_val_dst);
    metac_value_delete(p_val);
}END_TEST

struct test3_s{
    int y;
    char c;
} test3 = {.y = -10, .c = 97,};
METAC_GSYM_LINK(test3);
METAC_START_TEST(test3_sanity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test3);
    fail_unless(p_val != NULL, "wasn't able to get value");

    _check_serialization_(_json_string(p_val), "{\"y\":-10,\"c\":97}");

    metac_value_delete(p_val);
} END_TEST

int k = 0;
struct {
    int a;
    char b;
    struct {
        unsigned int d;
        struct {
            long e;
        };
    };
    union { /*need anon union to check that we're skipping it by default */
        struct {
            int f;
            long g;
        };
        int i;
    };
    struct {
        int j;
    }k;
    int l[3];
    int *p_m;
    int *p_k;
    union { /*need union to check that we're skipping it by default */
        int n;
        long o;
    }p;
}test4 = {.a = 0, .b = 0, .d = 0, .e = 0, .k = {.j = 0,},.l = {0, 0, 0}, .p_m = NULL, .p_k = &k, .p = {.n = 0},};
METAC_GSYM_LINK(test4);
METAC_START_TEST(test4_sanity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test4);
    fail_unless(p_val != NULL, "wasn't able to get value");

    char expected_s_pattern[] = "{\"a\":0,\"b\":0,\"d\":0,\"e\":0,\"k\":{\"j\":0},\"l\":[0,0,0],\"p_m\":null,\"p_k\":\"%p\",\"p\":{}}";
    char expected_s[sizeof(expected_s_pattern)+16];
    snprintf(expected_s, sizeof(expected_s), expected_s_pattern, test4.p_k);
    _check_serialization_(_json_string(p_val), expected_s);

    metac_value_delete(p_val);
} END_TEST


struct t5{
    int flex_size;
    struct test5_inner{
        int data;
        int more_data;
    } flex[] /*__attribute__((counted_by(flex_size))). see https://clang.llvm.org/docs/AttributeReference.html#field-attributes*/;
}*test5 = NULL;
METAC_GSYM_LINK(test5);

METAC_TAG_MAP_NEW(new_t5_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    /* start tags for all types */

    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct t5)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n = "flex"}),
            METAC_COUNT_BY(flex_size)
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END

METAC_START_TEST(test5_satnity) {
    metac_tag_map_t *p_tag_map = new_t5_tag_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");

    test5 = calloc(1, sizeof(*test5) + sizeof(struct test5_inner) * 3);
    test5->flex_size = 3;
    test5->flex[0].data = 1;
    test5->flex[1].data = 2;
    test5->flex[2].data = 3;

    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test5);
    metac_value_t * p_arr_val = metac_new_element_count_value(p_val, 1);
    metac_value_delete(p_val);

    _check_serialization_(
        _json_string_ex(p_arr_val, METAC_WMODE_shallow, p_tag_map),
        "[{\"flex_size\":3,\"flex\":[{\"data\":1,\"more_data\":0},{\"data\":2,\"more_data\":0},{\"data\":3,\"more_data\":0}]}]"
    );

    metac_value_delete(p_arr_val);
    free(test5);
    metac_tag_map_delete(p_tag_map);
} END_TEST

enum sgnd {
    eChar = 0,
    eShort,
    eInt,
    eLong,
    eLongLong,
};

struct test6{
    enum sgnd selector;
    union {
        char c;
        short s;
        int i;
        long l;
        long long ll;
    };
    enum sgnd sgnd_selector;
    union {
        char c;
        short s;
        int i;
        long l;
        long long ll;
    }sgnd;
}test6;
METAC_GSYM_LINK(test6);

METAC_TAG_MAP_NEW(new_t6_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    /* start tags for all types */

    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct test6)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.i = 1}),
            METAC_UNION_MEMBER_SELECT_BY(selector, 
                {.fld_val = eChar, .union_fld_name = "c"},
                {.fld_val = eShort, .union_fld_name = "s"},
                {.fld_val = eInt, .union_fld_name = "i"},
                {.fld_val = eLong, .union_fld_name = "l"},
                {.fld_val = eLongLong, .union_fld_name = "ll"},
            )
        )
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n = "sgnd"}),
            METAC_UNION_MEMBER_SELECT_BY(sgnd_selector, 
                {.fld_val = eChar, .union_fld_name = "c"},
                {.fld_val = eShort, .union_fld_name = "s"},
                {.fld_val = eInt, .union_fld_name = "i"},
                {.fld_val = eLong, .union_fld_name = "l"},
                {.fld_val = eLongLong, .union_fld_name = "ll"},
            )
        )
    METAC_TAG_MAP_ENTRY_END
METAC_TAG_MAP_END

/* was an api function before, now it's just part of the test*/
metac_value_t * metac_handle_unions(metac_value_t * p_union, metac_value_t * p_parent, char * pattern) {
    if (metac_value_has_members(p_parent) == 0) {
        return NULL;
    }
    if (metac_value_final_kind(p_union, NULL) != METAC_KND_union_type) {
        return NULL;
    }
    if (pattern == NULL) {
        pattern = "%s_selector";
    }

    char *flex_selector_field_name = _dgenfieldname(metac_value_name(p_union), pattern);
    if (flex_selector_field_name == NULL) {
        return NULL;
    }

    metac_num_t un_nr = metac_value_member_count(p_union);
    metac_num_t nr = metac_value_member_count(p_parent);
    metac_value_t * res = NULL;

    for (metac_num_t id = 0; id < nr ; ++id) {
        metac_value_t * field = metac_new_value_by_member_id(p_parent, id);
        if (field == NULL) {
            continue;
        }
        char * field_name = metac_value_name(field);
        if (field_name == NULL) {
            metac_value_delete(field);
            continue;
        }
        if (strcmp(field_name, flex_selector_field_name) == 0) {
            metac_num_t v;
            if (metac_value_num(field, &v) == 0) {
                if (v >=0 && v < un_nr) {
                    res = metac_new_value_by_member_id(p_union, v);
                    metac_value_delete(field);
                    break;
                }
            }
        }
        metac_value_delete(field);
    }
    free(flex_selector_field_name);

    return res;
}

int test6_artificial_handler(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    if (p_ev->type != METAC_RQVST_union_member) {
        return -(EFAULT);
    }
    if (metac_value_walker_hierarchy_level(p_hierarchy) < 1) {
        return -(EFAULT);
    }

    char *patterns[] = {
        "%s_selector",
        "selector", /* not patters are more generic - move the to the end */
    };
    for (size_t i = 0 ; i < sizeof(patterns)/sizeof(patterns[0]); ++i) {
        metac_value_t * res = metac_handle_unions(
            metac_value_walker_hierarchy_value(p_hierarchy, 0),
            metac_value_walker_hierarchy_value(p_hierarchy, 1),
            patterns[i]);
        if (res != NULL) {
            p_ev->p_return_value = res;
            return 0;
        }
    }
    return 0;
}
void test6_sanity_with_handler(metac_tag_map_t *p_tag_map) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test6);    
    
    memset(&test6, 0, sizeof(test6));
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_shallow, p_tag_map),
        "{\"selector\":\"eChar\",\"c\":0,\"sgnd_selector\":\"eChar\",\"sgnd\":{\"c\":0}}"
    );

    test6.selector = eInt;
    test6.i = -123456;
    test6.sgnd_selector = eShort;
    test6.sgnd.s = -12345;
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_shallow, p_tag_map),
        "{\"selector\":\"eInt\",\"i\":-123456,\"sgnd_selector\":\"eShort\",\"sgnd\":{\"s\":-12345}}"
    );

    metac_value_delete(p_val);
}
METAC_START_TEST(test6_satnity) {
    metac_tag_map_t *p_tag_map = metac_new_tag_map(METAC_TAG(.handler = test6_artificial_handler));
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");
    /* run with default handler */
    test6_sanity_with_handler(p_tag_map);

    metac_tag_map_delete(p_tag_map);
    /* run without default */
    p_tag_map = new_t6_tag_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");
    test6_sanity_with_handler(p_tag_map);

    metac_tag_map_delete(p_tag_map);
}END_TEST

struct t7_cntnt0 {
    int a;
    int b;
} t7_contnt0_0 = {.a = 1, .b = 2}, t7_contnt0_1 = {.a = -1, .b = -1000}, t7_contnt0_2[] = {{.a=4,},{.a=5},};

struct t7_cntnt1 {
    complex c;
}t7_contnt1_0 = {.c = 7.0 + I*3.4}, t7_contnt1_1 = {.c = 19.33 - I*0.4};

struct t7 {
    unsigned short data;
    // this is kind of Go interface thing, but very limited. also it's simial to union
    int content_type;
    int content_len;
    void *p_content;
}test7 = {.data = 0x555, .content_type = 0, .content_len = 2, .p_content = &t7_contnt0_2,};
METAC_GSYM_LINK(test7);

METAC_TAG_MAP_NEW(new_t7_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    /* start tags for all types */

    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct t7)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n = "p_content"}), 
            METAC_CAST_PTR(content_len, content_type, 
                {.fld_val = 0, .p_ptr_entry = METAC_ENTRY_OF(struct t7_cntnt0 *)}, 
                {.fld_val = 1, .p_ptr_entry = METAC_ENTRY_OF(struct t7_cntnt1 *)},
        ))
    METAC_TAG_MAP_ENTRY_END
METAC_TAG_MAP_END

/* part of artifitial handler - we removed those from API*/
/*
prefix is needed to make declloc uniuq if we put several of them on the same line 
there is an alternative - we can separate DECLLOC and everything else
*/
#define METAC_PTR_TO_ARRAY_OF_VALUE(_prefix_, _type_, _p_pointer_val_, _size_) ({ \
    metac_value_t * p_val = NULL; \
    WITH_METAC_DECLLOC(_prefix_##_l, _type_ * pl = NULL); \
    metac_value_t * p_l_val = METAC_VALUE_FROM_DECLLOC(_prefix_##_l, pl); \
    if (p_l_val != NULL) { \
        metac_value_pointer(_p_pointer_val_, (void**)&pl); \
        p_val = metac_new_element_count_value(p_l_val, _size_); \
        metac_value_delete(p_l_val); \
    } \
    p_val; })

/*
another working option with the approach that allows to use a single declloc.
it's important to use uniq prefixes (2nd param) for each line. otherwise there is no way to
find a proper type of variable. Example (see value_string_test.c test7 for more info):
        WITH_METAC_DECLLOC(dl,
            switch (content_type) {
            case 0: // convert pointer to flex array of the certain type
                res = METAC_PTR_TO_ARRAY_OF_VALUE_FROM_DECLLOC(dl, t0, struct t7_cntnt0, p_pointer_val, -1);
                break;
            case 1: 
                res = METAC_PTR_TO_ARRAY_OF_VALUE_FROM_DECLLOC(dl, t1, struct t7_cntnt1, p_pointer_val, -1);
                break;    
            }
        );
this was corrected with new approach. this the commit
*/
#define METAC_PTR_TO_ARRAY_OF_VALUE_FROM_DECLLOC(_type_, _p_pointer_val_, _size_) ({ \
    metac_value_t * _p_val_ = NULL; \
    _type_ * pl = NULL; \
    WITH_METAC_DECLLOC(dl, \
    metac_value_t * p_l_val = METAC_VALUE_FROM_DECLLOC(dl, pl)); \
    if (p_l_val != NULL) { \
        metac_value_pointer(_p_pointer_val_, (void**)&pl); \
        _p_val_ = metac_new_element_count_value(p_l_val, _size_); \
        metac_value_delete(p_l_val); \
    } \
    _p_val_; })

int test7_artifitial_handler(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    //handle_count_by isn't enough here, because we also want to change the type from void*
    //handle_cast_by(p_content, int, int, content_type, case 0: return ...; )
    do {
        if (p_ev->type == METAC_RQVST_pointer_array_count && 
            metac_value_walker_hierarchy_level(p_hierarchy) > 0) { /* we need to have some siblings */

            metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
            metac_value_t *p_parent_val = metac_value_walker_hierarchy_value(p_hierarchy, 1);
            if (p_val == NULL || p_parent_val == NULL || metac_value_has_members(p_parent_val) == 0) { \
                break;
            }
            char * field_name = metac_value_name(p_val);
            if (field_name == NULL || strcmp(field_name, "p_content") != 0) {
                break;
            }

            for (metac_num_t i = 0; i < metac_value_member_count(p_parent_val); ++i) {
                metac_value_t *p_sibling_val = metac_new_value_by_member_id(p_parent_val, i);
                if (metac_value_name(p_sibling_val) == NULL ||
                    strcmp(metac_value_name(p_sibling_val), "content_type") !=0 ) {
                    metac_value_delete(p_sibling_val);
                    continue;
                }

                int content_type;
                if (metac_value_int(p_sibling_val, &content_type) != 0) {
                    metac_value_delete(p_sibling_val);
                    return 0;
                }
                metac_value_delete(p_sibling_val);
                switch (content_type) {
                    case 0: { // convert pointer to flex array of the certain type
                            p_ev->p_return_value = METAC_PTR_TO_ARRAY_OF_VALUE_FROM_DECLLOC(struct t7_cntnt0, p_val, -1);
                        return 0;
                    }
                    case 1: {
                            p_ev->p_return_value = METAC_PTR_TO_ARRAY_OF_VALUE_FROM_DECLLOC(struct t7_cntnt1, p_val, -1);
                        return 0;
                    }
                }
                return 0;
            }
        }
    } while(0);
    do {
        if (p_ev->type == METAC_RQVST_flex_array_count && 
            metac_value_walker_hierarchy_level(p_hierarchy) > 1) { /* we need to have some siblings */

            metac_value_t *p_array_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
            metac_value_t *p_pointer_val = metac_value_walker_hierarchy_value(p_hierarchy, 1);
            metac_value_t *p_parent_struct_val = metac_value_walker_hierarchy_value(p_hierarchy, 2);
            if (p_array_val == NULL || p_pointer_val == NULL || p_parent_struct_val == NULL) {
                break;
            }
            char * field_name = metac_value_name(p_pointer_val);
            if (strcmp(field_name, "p_content") != 0 || metac_value_member_count(p_parent_struct_val) != 4) {
                break;
            }
            for (metac_num_t i = 0; i < metac_value_member_count(p_parent_struct_val); ++i) {
                metac_value_t *p_sibling_val = metac_new_value_by_member_id(p_parent_struct_val, i);
                if (metac_value_name(p_sibling_val) == NULL ||
                    strcmp(metac_value_name(p_sibling_val), "content_len") !=0 ) {
                    metac_value_delete(p_sibling_val);
                    continue;
                }

                metac_num_t content_len;
                if (metac_value_num(p_sibling_val, &content_len) != 0) {
                    metac_value_delete(p_sibling_val);
                    return 0;
                }
                metac_value_delete(p_sibling_val);

                p_ev->p_return_value = metac_new_element_count_value(p_array_val, content_len);
                return 0;
            }
        }
    }while(0);
    return 0;
}

void test7_sanity_with_handler(metac_tag_map_t *p_tag_map) {
    char exp_buf[128];
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test7);

    test7.data = 0x555;
    test7.content_type = 0;
    test7.content_len = 2;
    test7.p_content = &t7_contnt0_2;

    snprintf(exp_buf, sizeof(exp_buf), "{\"data\":1365,\"content_type\":0,\"content_len\":2,\"p_content\":\"%p\"}", test7.p_content); 
    _check_serialization_(
        _json_string(p_val),
        exp_buf
    );

    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, p_tag_map),
        "{\"data\":1365,\"content_type\":0,\"content_len\":2,\"p_content\":[{\"a\":4,\"b\":0},{\"a\":5,\"b\":0}]}"
    );

    test7.data = 555;
    test7.content_len = 1;
    test7.p_content = &t7_contnt0_0;
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, p_tag_map),
        "{\"data\":555,\"content_type\":0,\"content_len\":1,\"p_content\":{\"a\":1,\"b\":2}}"
    );

    test7.data = 777;
    test7.content_len = 1;
    test7.p_content = &t7_contnt0_1;
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, p_tag_map),
        "{\"data\":777,\"content_type\":0,\"content_len\":1,\"p_content\":{\"a\":-1,\"b\":-1000}}"
    );

    test7.data = 999;
    test7.content_type = 1;
    test7.content_len = 1;
    test7.p_content = &t7_contnt1_0;
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, p_tag_map),
        "{\"data\":999,\"content_type\":1,\"content_len\":1,\"p_content\":{\"c\":\"7.000000 + I * 3.400000\"}}"
    );

    test7.data = 888;
    test7.content_type = 1;
    test7.content_len = 1;
    test7.p_content = &t7_contnt1_1;
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, p_tag_map),
        "{\"data\":888,\"content_type\":1,\"content_len\":1,\"p_content\":{\"c\":\"19.330000 - I * 0.400000\"}}"
    );

    test7.data = 1000;
    test7.content_type = 2;
    test7.content_len = 1;
    test7.p_content = &t7_contnt1_1;

    /* check fallback to shallow if it's void* */
    snprintf(exp_buf, sizeof(exp_buf), "{\"data\":1000,\"content_type\":2,\"content_len\":1,\"p_content\":\"%p\"}", test7.p_content);
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, p_tag_map),
        exp_buf
    );

    metac_value_delete(p_val);
}

METAC_START_TEST(test7_satnity) {
    metac_tag_map_t *p_tag_map;
    p_tag_map = metac_new_tag_map(METAC_TAG(.handler = test7_artifitial_handler));
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");
    /* run with default handler */
    test7_sanity_with_handler(p_tag_map);

    metac_tag_map_delete(p_tag_map);
    /* run without default */
    p_tag_map = new_t7_tag_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");
    test7_sanity_with_handler(p_tag_map);

    metac_tag_map_delete(p_tag_map);
}END_TEST

struct t8_list_itm {
    int data;
    struct t8_list_itm * next;   
}*t8_head = (struct t8_list_itm []){{.data = 0, .next = (struct t8_list_itm []){{.data = 1, .next = NULL,},},},};
METAC_GSYM_LINK(t8_head);
METAC_START_TEST(test8_satnity) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(t8_head);

    // without loop
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, NULL),
        "{\"data\":0,\"next\":{\"data\":1,\"next\":null}}"
    );

    // create loop
    t8_head->next->next = t8_head;
    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, NULL),
        NULL
    );

    metac_value_delete(p_val);
}END_TEST

char * test9 = "some data";
METAC_GSYM_LINK(test9);
METAC_TAG_MAP_NEW(new_t9_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    /* start tags for all types */
    METAC_TAG_MAP_ENTRY(METAC_GSYM_LINK_ENTRY(test9))
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_SELF,
            METAC_ZERO_ENDED_STRING()
        )
    METAC_TAG_MAP_ENTRY_END
METAC_TAG_MAP_END
METAC_START_TEST(test9_satnity) {
    metac_tag_map_t * p_tagmap = new_t9_tag_map();
    fail_unless(p_tagmap != NULL, "t9 tagmap is NULL");
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(test9);

    _check_serialization_(
        _json_string_ex(p_val, METAC_WMODE_deep, p_tagmap),
        "\"some data\""
    );

    metac_value_delete(p_val);
    metac_tag_map_delete(p_tagmap);
}END_TEST


//TODO: rework/remove after this point
struct test_cjson_array_struct {
    int simple_arr[3];
};
struct test_cjson_array_struct g_test_cjson_array_struct = { .simple_arr = {10, 20, 30} };
METAC_GSYM_LINK(g_test_cjson_array_struct);

METAC_START_TEST(cjson_backend_array_struct) {
    metac_value_t* p_val = METAC_VALUE_FROM_LINK(g_test_cjson_array_struct);
    fail_unless(p_val != NULL, "Wasn't able to get value for g_test_cjson_array_struct");

    struct cJSON* json = metac_value_to_cjson(p_val, METAC_WMODE_deep, NULL);
    fail_unless(json != NULL, "metac_value_to_cjson returned NULL");

    char* json_str = cJSON_PrintUnformatted(json);
    fail_unless(json_str != NULL, "cJSON_PrintUnformatted returned NULL");

    const char* expected_str = "{\"simple_arr\":[10,20,30]}";
    fail_unless(strcmp(json_str, expected_str) == 0, "Expected '%s', got '%s'", expected_str, json_str);

    free(json_str);
    cJSON_Delete(json);
    metac_value_delete(p_val);
} END_TEST


struct test_cjson_linked_list {
    int data;
    struct test_cjson_linked_list* next;
};

// Create a small linked list for testing
struct test_cjson_linked_list g_test_cjson_list_node2 = { .data = 20, .next = NULL };
struct test_cjson_linked_list g_test_cjson_list_node1 = { .data = 10, .next = &g_test_cjson_list_node2 };
struct test_cjson_linked_list* g_test_cjson_list_head = &g_test_cjson_list_node1;

METAC_GSYM_LINK(g_test_cjson_list_head);

METAC_START_TEST(cjson_backend_linked_list) {
    metac_value_t* p_val = METAC_VALUE_FROM_LINK(g_test_cjson_list_head);
    fail_unless(p_val != NULL, "Wasn't able to get value for g_test_cjson_list_head");

    struct cJSON* json = metac_value_to_cjson(p_val, METAC_WMODE_deep, NULL);
    fail_unless(json != NULL, "metac_value_to_cjson returned NULL");

    char* json_str = cJSON_PrintUnformatted(json);
    fail_unless(json_str != NULL, "cJSON_PrintUnformatted returned NULL");

    const char* expected_str = "{\"data\":10,\"next\":{\"data\":20,\"next\":null}}";
    fail_unless(strcmp(json_str, expected_str) == 0, "Expected '%s', got '%s'", expected_str, json_str);

    free(json_str);
    cJSON_Delete(json);
    metac_value_delete(p_val);
} END_TEST

// Test for circular dependencies
struct test_cjson_circular {
    int data;
    struct test_cjson_circular* loop;
};
struct test_cjson_circular g_test_cjson_circ1 = { .data = 1, .loop = NULL };
struct test_cjson_circular g_test_cjson_circ2 = { .data = 2, .loop = &g_test_cjson_circ1 };
METAC_GSYM_LINK(g_test_cjson_circ1);

METAC_START_TEST(cjson_backend_circular_ref) {
    //Create the loop
    g_test_cjson_circ1.loop = &g_test_cjson_circ2;

    metac_value_t* p_val = METAC_VALUE_FROM_LINK(g_test_cjson_circ1);
    fail_unless(p_val != NULL, "Wasn't able to get value for g_test_cjson_circ1");
    
    struct cJSON* json = metac_value_to_cjson(p_val, METAC_WMODE_deep, NULL);
    fail_unless(json == NULL, "metac_value_to_cjson returned NULL");

    cJSON_Delete(json);
    metac_value_delete(p_val);
} END_TEST


enum test_cjson_sgnd {
    e_cjson_Char = 0,
    e_cjson_Short,
    e_cjson_Int,
};

struct test_cjson_tagged_union {
    enum test_cjson_sgnd selector;
    union {
        char c;
        short s;
        int i;
    } data;
};

METAC_TAG_MAP_NEW(g_test_cjson_tagged_union_map, NULL, {.mask =
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct test_cjson_tagged_union)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n = "data"}),
            METAC_UNION_MEMBER_SELECT_BY(selector,
                {.fld_val = e_cjson_Char, .union_fld_name = "c"},
                {.fld_val = e_cjson_Short, .union_fld_name = "s"},
                {.fld_val = e_cjson_Int, .union_fld_name = "i"}
            )
        )
    METAC_TAG_MAP_ENTRY_END
METAC_TAG_MAP_END

struct test_cjson_tagged_union g_test_cjson_tagged_union_var;
METAC_GSYM_LINK(g_test_cjson_tagged_union_var);

METAC_START_TEST(cjson_backend_deserialization_array) {
    const char* json_str = "{\"simple_arr\":[11,22,33]}";
    cJSON* json = cJSON_Parse(json_str);
    fail_unless(json != NULL, "cJSON_Parse failed");

    struct test_cjson_array_struct target_struct = {0};
    metac_entry_t* p_entry = METAC_GSYM_LINK_ENTRY(g_test_cjson_array_struct);
    metac_value_t* p_val = metac_new_value(p_entry, &target_struct);
    fail_unless(p_val != NULL, "metac_new_value failed for target_struct");

    int result = metac_value_from_cjson(p_val, json, NULL);
    fail_unless(result == 0, "metac_value_from_cjson failed");

    fail_unless(target_struct.simple_arr[0] == 11, "Array[0] should be 11, but is %d", target_struct.simple_arr[0]);
    fail_unless(target_struct.simple_arr[1] == 22, "Array[1] should be 22, but is %d", target_struct.simple_arr[1]);
    fail_unless(target_struct.simple_arr[2] == 33, "Array[2] should be 33, but is %d", target_struct.simple_arr[2]);

    metac_value_delete(p_val);
    cJSON_Delete(json);
}

METAC_START_TEST(cjson_backend_deserialization_struct) {
    const char* json_str = "{\"y\":-99,\"c\":21}";
    cJSON* json = cJSON_Parse(json_str);
    fail_unless(json != NULL, "cJSON_Parse failed");

    struct test3_s target_struct;
    metac_entry_t* p_entry = METAC_GSYM_LINK_ENTRY(test3);
    metac_value_t* p_val = metac_new_value(p_entry, &target_struct);
    fail_unless(p_val != NULL, "metac_new_value failed for target_struct");

    int result = metac_value_from_cjson(p_val, json, NULL);
    fail_unless(result == 0, "metac_value_from_cjson failed");

    fail_unless(target_struct.y == -99, "Deserialization error: y should be -99, but is %d", target_struct.y);
    fail_unless(target_struct.c == 21, "Deserialization error: c should be 'Q', but is %c", target_struct.c);

    metac_value_delete(p_val);
    cJSON_Delete(json);
}

METAC_START_TEST(cjson_backend_deserialization_union) {
    metac_tag_map_t* p_tag_map = g_test_cjson_tagged_union_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap for tagged union");

    metac_value_t* p_val = METAC_VALUE_FROM_LINK(g_test_cjson_tagged_union_var);
    fail_unless(p_val != NULL, "Wasn't able to get value for g_test_cjson_tagged_union_var");

    // Test with char
    g_test_cjson_tagged_union_var.selector = e_cjson_Char;
    g_test_cjson_tagged_union_var.data.c = 90;
    
    struct cJSON* json_char = metac_value_to_cjson(p_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(json_char != NULL, "[char] metac_value_to_cjson returned NULL");
    char* json_str_char = cJSON_PrintUnformatted(json_char);
    const char* expected_str_char = "{\"selector\":\"e_cjson_Char\",\"data\":{\"c\":90}}";
    fail_unless(strcmp(json_str_char, expected_str_char) == 0, "[char] Expected '%s', got '%s'", expected_str_char, json_str_char);
    free(json_str_char);
    cJSON_Delete(json_char);

    // Test with int
    g_test_cjson_tagged_union_var.selector = e_cjson_Int;
    g_test_cjson_tagged_union_var.data.i = 999;

    struct cJSON* json_int = metac_value_to_cjson(p_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(json_int != NULL, "[int] metac_value_to_cjson returned NULL");
    char* json_str_int = cJSON_PrintUnformatted(json_int);
    const char* expected_str_int = "{\"selector\":\"e_cjson_Int\",\"data\":{\"i\":999}}";
    fail_unless(strcmp(json_str_int, expected_str_int) == 0, "[int] Expected '%s', got '%s'", expected_str_int, json_str_int);
    free(json_str_int);
    cJSON_Delete(json_int);

    metac_value_delete(p_val);
    metac_tag_map_delete(p_tag_map);
}

#endif // WITH_CJSON
