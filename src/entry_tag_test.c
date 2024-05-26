#include "metac/test.h"

#include "entry.c"
#include "entry_tag.c"
#include "entry_db.c"
#include "hashmap.c"
#include "iterator.c"
#include "value.c"
#include "value_base_type.c"

struct x {
    int selector;
    union {
        char c;
        short s;
        int i;
        long l;
        long long ll;
    };
    int sgnd_selector;
    union {
        char c;
        short s;
        int i;
        long l;
        long long ll;
    }sgnd;
    struct {
        int a;
        int b1;
        struct {
            char b2;
        };
    };
    struct {
        int c;
        long d;
    }w;
};

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

void _test_fn1(int len, char * data){
    for (int i = 0 ; i< 0; ++i) {
        printf("%c", data[i]);
    }
}
METAC_GSYM_LINK(_test_fn1);

METAC_TAG_MAP_NEW(new_module_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_subprogram_parameter) | 
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

    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct x)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.i = 1}),
            METAC_TAG_QSTRING("nondefault")
            METAC_UNION_MEMBER_SELECT_BY(selector, 
                {.fld_val = 0, .union_fld_name = "c"},
                {.fld_val = 1, .union_fld_name = "s"},
                {.fld_val = 2, .union_fld_name = "i"},
                {.fld_val = 3, .union_fld_name = "l"},
                {.fld_val = 4, .union_fld_name = "ll"},
            )
        )
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n = "sgnd"}),
            METAC_TAG_QSTRING("yaml:test") /*this will generate .p_tagstring = "\"yaml:test\"" */
            METAC_UNION_MEMBER_SELECT_BY(sgnd_selector, 
                {.fld_val = 0, .union_fld_name = "c"},
                {.fld_val = 1, .union_fld_name = "s"},
                {.fld_val = 2, .union_fld_name = "i"},
                {.fld_val = 3, .union_fld_name = "l"},
                {.fld_val = 4, .union_fld_name = "ll"},
            )
        )
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n = "w"}),
            METAC_TAG_QSTRING(json:",omitempty" yaml:",omitempty") /* will be "json:\",omitempty\" yaml:\",omitempty\""*/
        )
    METAC_TAG_MAP_ENTRY_END

    METAC_TAG_MAP_ENTRY(METAC_GSYM_LINK_ENTRY(_test_fn1))
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_PARAMETER({.n= "data"}),
            METAC_COUNT_BY(len)
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END

METAC_START_TEST(test_metac_entry_tag) {
    metac_tag_map_t *p_tag_map = new_module_tag_map();
    fail_unless(p_tag_map != NULL, "expected non-null tag-map");

    WITH_METAC_DECLLOC(loc, struct x val1; struct t7 val2;);

    metac_entry_tag_t * p_tag;
    metac_entry_t * p_res_entry;
    metac_entry_t *p_entry;
    p_entry = metac_entry_final_entry(METAC_ENTRY_FROM_DECLLOC(loc, val1), NULL);
    fail_unless(p_entry != NULL, "expected non-null");

    p_res_entry = METAC_ENTRY_BY_MEMBER_IDS(p_entry, 0, {.i=1});
    fail_unless(p_res_entry!= NULL, "expected non null");
    fail_unless(p_res_entry->kind == METAC_KND_member && p_res_entry->name == NULL, "expected anon member got %s", p_res_entry->name);
    p_tag = metac_tag_map_tag(p_tag_map, p_res_entry);
    fail_unless(p_tag != NULL, "expected non null tag");
    fail_unless(p_tag->p_tagstring != NULL && strcmp(p_tag->p_tagstring, "\"nondefault\"") == 0, "expected nondefault tag, got %s", p_tag->p_tagstring);

    p_res_entry = METAC_ENTRY_BY_MEMBER_IDS(p_entry, 1, {.n="selector"});
    fail_unless(p_res_entry!= NULL, "expected non null");
    fail_unless((p_res_entry->kind == METAC_KND_base_type) && (strcmp(p_res_entry->name, "int") == 0), 
        "expected anon union, got %s", p_res_entry->name);
    p_tag = metac_tag_map_tag(p_tag_map, p_res_entry);
    fail_unless(p_tag == NULL, "expected null tag");

    // another structure 
    p_entry = metac_entry_final_entry(METAC_ENTRY_FROM_DECLLOC(loc, val2), NULL);
    fail_unless(p_entry != NULL, "expected non-null");

    p_res_entry = METAC_ENTRY_BY_MEMBER_IDS(p_entry, 0, {.n = "p_content"});
    fail_unless(p_res_entry!= NULL, "expected non null");    
    p_tag = metac_tag_map_tag(p_tag_map, p_res_entry);
    fail_unless(p_tag != NULL, "expected non null tag");

    // function
    p_entry = METAC_GSYM_LINK_ENTRY(_test_fn1);
    p_res_entry = METAC_ENTRY_BY_PARAMETER_IDS(p_entry, 0, {.n = "data"});
    fail_unless(p_res_entry!= NULL, "expected non null");    
    p_tag = metac_tag_map_tag(p_tag_map, p_res_entry);
    fail_unless(p_tag != NULL, "expected non null tag");


    metac_tag_map_delete(p_tag_map);
}END_TEST

METAC_START_TEST(test_tag_string_lookup) {
    struct tc_string_lookup {
        metac_entry_tag_t * p_in;
        metac_name_t key;
        metac_name_t expected;
    }tcs[] = {
        {
            .p_in = METAC_TAG(METAC_TAG_QSTRING(test:"x")),
            .key = "test",
            .expected = "x",
        },
        {
            .p_in = METAC_TAG(METAC_TAG_QSTRING(species:"gopher" color:"blue" sum:"black")),
            .key = "color",
            .expected = "blue",
        },
        {
            .p_in = METAC_TAG(METAC_TAG_QSTRING(species:"gopher" color:"blue sum:\"black\"")),
            .key = "color",
            .expected = "blue sum:\\\"black\\\"",
        },
        /* error handling */
        {
            .p_in = NULL,
            .key = "test",
            .expected = NULL,
        },
        {
            .p_in = METAC_TAG(),
            .key = "test",
            .expected = NULL,
        },
        {
            .p_in = METAC_TAG(METAC_TAG_QSTRING(test:"x")),
            .key = NULL,
            .expected = NULL,
        },
        {
            .p_in = METAC_TAG(METAC_TAG_QSTRING(xxx:yyy species:gopher)),
            .key = "species",
            .expected = NULL,
        },
        /* partially correct string */
        {
            .p_in = METAC_TAG(METAC_TAG_QSTRING(species:"gopher" color color:"blue" sum:"black")),
            .key = "color",
            .expected = "blue",
        },
    };
    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        metac_name_t res = metac_entry_tag_string_lookup(tcs[tc_inx].p_in, tcs[tc_inx].key);

        fail_unless( ((tcs[tc_inx].expected == NULL) == (res == NULL)), "got %s, expected %s", res, tcs[tc_inx].expected);
        if (res != NULL) {
            fail_unless(strcmp(res, tcs[tc_inx].expected) == 0, "got %s, expected %s", res, tcs[tc_inx].expected);
        }

        if (res != NULL) {
            free(res);
        }
    }
}END_TEST
