#include "metac/test.h"

#include "entry.c"
#include "entry_cdecl.c"
#include "entry_db.c"
#include "entry_tag.c"
#include "iterator.c"
#include "hashmap.c"
#include "value.c"
#include "value_base_type.c"
#include "value_deep.c"
#include "value_string.c"
#include "value_with_args.c"

struct list_itm {
    int data;
    struct list_itm * next;   
};

struct list_itm *t1_head = (struct list_itm []){{.data = 0, .next = (struct list_itm []){{.data = 1, .next = NULL,},},},};
METAC_GSYM_LINK(t1_head);
METAC_START_TEST(test1_check_map) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(t1_head);
    fail_unless(p_val != NULL, "p_val is NULL");

    struct metac_memory_map * p_map  = metac_new_value_memory_map_ex(p_val, 
        NULL, NULL);
    fail_unless(p_map != NULL, "p_map is NULL");

    metac_value_memory_map_delete(p_map);
    metac_value_delete(p_val);
}END_TEST

static int _t2_free_count = 0;
static void _t2_free_fn(void *ptr){
    ++_t2_free_count;
}
struct list_itm *t2_head = (struct list_itm []){{.data = 0, .next = (struct list_itm []){{.data = 1, .next = NULL,},},},};
METAC_GSYM_LINK(t2_head);
METAC_START_TEST(test2_free_with_loop) {
    metac_value_t * p_val = METAC_VALUE_FROM_LINK(t2_head);
    fail_unless(p_val != NULL, "p_val is NULL");

    // create loop
    t2_head->next->next = t2_head;

    _t2_free_count = 0;
    fail_unless(metac_value_free_ex(p_val, NULL, _t2_free_fn, NULL)!=0, "failed to free t2_head");
    fail_unless(_t2_free_count == 2, "number of deletions was expected 2, got %i", _t2_free_count);
    metac_value_delete(p_val);
}END_TEST

static Hashmap* allocated_memory_tracker = NULL;
static int _hash_memory_tracker(void* key) {
    return hashmapHash(&key, sizeof(void*));
}
static bool _equals_memory_tracker(void* keyA, void* keyB) {
    return keyA == keyB;
}
static int _init_memory_tracker() {
    if (allocated_memory_tracker != NULL) {
        return -EALREADY;
    }
    allocated_memory_tracker = hashmapCreate(1000, _hash_memory_tracker, _equals_memory_tracker);
    if (allocated_memory_tracker == NULL) {
        return -ENOMEM;
    }
    return 0;
}
static int _memory_tracker_has_allocated_mem() {
    return hashmapSize(allocated_memory_tracker);
}
static void _uninit_memory_tracker() {
    if (allocated_memory_tracker == NULL) {
        return;
    }
    hashmapFree(allocated_memory_tracker);
    allocated_memory_tracker = NULL;
}

static void *tcalloc(size_t nmemb, size_t size) {
    void * ptr = calloc(nmemb, size);

    if (allocated_memory_tracker != NULL){
        hashmapPut(allocated_memory_tracker, ptr, ptr, NULL);
    }
    return ptr;
}

static void tfree(void *ptr) {
    if (allocated_memory_tracker != NULL) {
        if (hashmapRemove(allocated_memory_tracker, ptr) == NULL) {
            fail("deleting ptr that wasn't allocated %p", ptr);
        }
    }
    free(ptr);
}

struct t3_user{
    int user_id;
    int age;
};

struct t3_users {
    int users_count;
    struct t3_user *users;
};

struct t3_roles {
    int role_id;
    struct t3_user * p_user; /*weak pointer*/
};

struct t3 {
    struct t3_users * p_users;
    struct {
        int count;
        struct t3_roles * p;
    } roles;
};

METAC_TAG_MAP_NEW(new_t3_tag_map, NULL, {.mask = 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_subprogram_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct t3_users)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n= "users"}),
            METAC_COUNT_BY(users_count)
        )
    METAC_TAG_MAP_ENTRY_END

    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct t3)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n="roles"},{.n="p"}),
            METAC_COUNT_BY(count)
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END


struct t3 * _create_t3_test_data() {
    struct t3 *p = NULL;
    p = tcalloc(1, sizeof(*p));
    p->p_users = tcalloc(1, sizeof(*p->p_users));
    p->p_users->users_count = 4;
    p->p_users->users = tcalloc(p->p_users->users_count, sizeof(*p->p_users->users));
    for (int i = 0; i < p->p_users->users_count; ++i) {
        p->p_users->users[i].user_id = i;
        p->p_users->users[i].age = i+22;
    }
    p->roles.count = 2;
    p->roles.p = tcalloc(p->roles.count, sizeof(*p->roles.p));
    for (int i = 0; i < p->roles.count; ++i) {
        p->roles.p[i].role_id = i;
        p->roles.p[i].p_user = &p->p_users->users[i*2];
    }
    return p;
}

METAC_START_TEST(test_free) {
    metac_tag_map_t *p_tag_map = new_t3_tag_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");

    char *s, *expected_s;
    fail_unless(_init_memory_tracker()==0, "couldn't created mem tracker");

    WITH_METAC_DECLLOC(t3, struct t3 *p_t3 = _create_t3_test_data());
    metac_value_t *t3_val = METAC_VALUE_FROM_DECLLOC(t3, p_t3);
    fail_unless(t3_val != NULL, "got NULL value");
    
    // checking our hanlder with string
    expected_s = 
    "(struct t3 []){"
        "{"
            ".p_users = (struct t3_users []){"
                "{"
                    ".users_count = 4, "
                    ".users = (struct t3_user []){"
                        "{.user_id = 0, .age = 22,}, "
                        "{.user_id = 1, .age = 23,}, "
                        "{.user_id = 2, .age = 24,}, "
                        "{.user_id = 3, .age = 25,},"
                    "},"
                "},"
            "}, "
            ".roles = {"
                ".count = 2, "
                ".p = (struct t3_roles []){"
                    "{"
                        ".role_id = 0, "
                        ".p_user = (struct t3_user []){"
                            "{.user_id = 0, .age = 22,},"
                        "},"
                    "}, "
                    "{"
                        ".role_id = 1, "
                        ".p_user = (struct t3_user []){"
                            "{.user_id = 2, .age = 24,}"
                        ",}"
                    ",}"
                ",}"
            ",}"
        ",}"
    ",}";
    s = metac_value_string_ex(t3_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(s != NULL, "got NULL string");
    fail_unless(strcmp(expected_s, s)==0, "expeted %s, got %s",expected_s, s);
    free(s);

    fail_unless(metac_value_free_ex(t3_val, NULL, tfree, p_tag_map)!=0, "deletion didn't happen");

    metac_value_delete(t3_val);

    fail_unless(_memory_tracker_has_allocated_mem() == 0, "tracker repoted memory %i", _memory_tracker_has_allocated_mem());
    _uninit_memory_tracker();
    metac_tag_map_delete(p_tag_map);
}END_TEST

METAC_START_TEST(test_copy_tree) {
    metac_tag_map_t *p_tag_map = new_t3_tag_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");

    char *s, *expected_s;
    fail_unless(_init_memory_tracker()==0, "couldn't created mem tracker");

    WITH_METAC_DECLLOC(location, struct t3 *p_src = _create_t3_test_data(), *p_dst = NULL);
    metac_value_t *p_src_val = METAC_VALUE_FROM_DECLLOC(location, p_src), *p_dst_val = METAC_VALUE_FROM_DECLLOC(location, p_dst);
    fail_unless(p_src_val != NULL, "got src NULL value");
    fail_unless(p_dst_val != NULL, "got dst NULL value");

    expected_s = metac_value_string_ex(p_src_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(expected_s != NULL, "expected_s is NULL");

    fail_unless(
        metac_value_copy_ex(p_src_val, p_dst_val,
            (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_fail}},
            tcalloc, tfree,
            p_tag_map) 
        != NULL,
        "copy failed"
    );

    s = metac_value_string_ex(p_dst_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(s != NULL, "s is NULL");

    fail_unless(strcmp(expected_s, s) == 0, "expected %s, got %s", expected_s, s);
    free(s);
    free(expected_s);

    fail_unless(metac_value_free_ex(p_src_val, NULL, tfree, p_tag_map)!=0, "src deletion didn't happen");
    fail_unless(metac_value_free_ex(p_dst_val, NULL, tfree, p_tag_map)!=0, "dst deletion didn't happen");
    metac_value_delete(p_src_val);
    metac_value_delete(p_dst_val);

    fail_unless(_memory_tracker_has_allocated_mem() == 0, "tracker repoted memory %i", _memory_tracker_has_allocated_mem());
    _uninit_memory_tracker();
    metac_tag_map_delete(p_tag_map);
}END_TEST

METAC_START_TEST(test_copy_dag) {
    metac_tag_map_t *p_tag_map = new_t3_tag_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");

    char *s, *expected_s;
    fail_unless(_init_memory_tracker()==0, "couldn't created mem tracker");

    WITH_METAC_DECLLOC(location, struct t3 *p_src = _create_t3_test_data(), *p_dst = NULL);
    metac_value_t *p_src_val = METAC_VALUE_FROM_DECLLOC(location, p_src), *p_dst_val = METAC_VALUE_FROM_DECLLOC(location, p_dst);
    fail_unless(p_src_val != NULL, "got src NULL value");
    fail_unless(p_dst_val != NULL, "got dst NULL value");

    expected_s = metac_value_string_ex(p_src_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(expected_s != NULL, "expected_s is NULL");

    fail_unless(
        metac_value_copy_ex(p_src_val, p_dst_val,
            NULL,
            tcalloc, tfree,
            p_tag_map) 
        != NULL,
        "copy failed"
    );

    s = metac_value_string_ex(p_dst_val, METAC_WMODE_deep, p_tag_map);
    fail_unless(s != NULL, "s is NULL");

    fail_unless(strcmp(expected_s, s) == 0, "expected %s, got %s", expected_s, s);
    free(s);
    free(expected_s);

    fail_unless(metac_value_free_ex(p_src_val, NULL, tfree, p_tag_map)!=0, "src deletion didn't happen");
    fail_unless(metac_value_free_ex(p_dst_val, NULL, tfree, p_tag_map)!=0, "dst deletion didn't happen");
    metac_value_delete(p_src_val);
    metac_value_delete(p_dst_val);

    fail_unless(_memory_tracker_has_allocated_mem() == 0, "tracker repoted memory %i", _memory_tracker_has_allocated_mem());
    _uninit_memory_tracker();
    metac_tag_map_delete(p_tag_map);
}END_TEST


struct list_itm *t3_src = (struct list_itm []){{.data = 0, .next = (struct list_itm []){{.data = 1, .next = NULL,},},},};
struct list_itm *t3_dst = NULL;
METAC_GSYM_LINK(t3_src);
METAC_GSYM_LINK(t3_dst);
METAC_START_TEST(test_copy_and_free_without_loop) {
    char *s, *expected_s;
    fail_unless(_init_memory_tracker()==0, "couldn't created mem tracker");

    metac_value_t * p_src_val = METAC_VALUE_FROM_LINK(t3_src);
    fail_unless(p_src_val != NULL, "p_src_val is NULL");
    metac_value_t * p_dst_val = METAC_VALUE_FROM_LINK(t3_dst);
    fail_unless(p_dst_val != NULL, "p_dst_val is NULL");

    fail_unless(
        metac_value_copy_ex(p_src_val, p_dst_val,
            (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_fail}},
            tcalloc, tfree,
            NULL) 
        != NULL,
        "copy failed"
    );
    s = metac_value_string_ex(p_dst_val, METAC_WMODE_deep, NULL);
    fail_unless(s != NULL, "got NULL string");
    expected_s = "(struct list_itm []){{.data = 0, .next = (struct list_itm []){{.data = 1, .next = NULL,},},},}";
    fail_unless(strcmp(s, expected_s) == 0, "expected %s, got %s", expected_s, s);
    free(s);

    fail_unless(metac_value_free_ex(p_dst_val, NULL, tfree, NULL) != 0, "failed to free p_dst_val");
    metac_value_delete(p_dst_val);
    metac_value_delete(p_src_val);

    fail_unless(_memory_tracker_has_allocated_mem() == 0, "tracker repoted memory %i", _memory_tracker_has_allocated_mem());
    _uninit_memory_tracker();
}END_TEST

struct list_itm *t4_src = (struct list_itm []){{.data = 0, .next = (struct list_itm []){{.data = 1, .next = NULL,},},},};
struct list_itm *t4_dst = NULL;
METAC_GSYM_LINK(t4_src);
METAC_GSYM_LINK(t4_dst);
METAC_START_TEST(test_copy_and_free_with_loop) {
    fail_unless(_init_memory_tracker()==0, "couldn't created mem tracker");

    metac_value_t * p_src_val = METAC_VALUE_FROM_LINK(t4_src);
    fail_unless(p_src_val != NULL, "p_src_val is NULL");
    metac_value_t * p_dst_val = METAC_VALUE_FROM_LINK(t4_dst);
    fail_unless(p_dst_val != NULL, "p_dst_val is NULL");

    // create loop
    t4_src->next->next = t4_src;

    fail_unless(
        metac_value_copy_ex(p_src_val, p_dst_val,
            (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_fail}},
            tcalloc, tfree,
            NULL) 
        != NULL,
        "copy failed"
    );

    fail_unless(metac_value_free_ex(p_dst_val, NULL, tfree, NULL) != 0, "failed to free p_dst_val");
    metac_value_delete(p_dst_val);
    metac_value_delete(p_src_val);

    fail_unless(_memory_tracker_has_allocated_mem() == 0, "tracker repoted memory %i", _memory_tracker_has_allocated_mem());
    _uninit_memory_tracker();
}END_TEST

/*test base types and enums*/
struct _test_metac_value_equal_ex_1 {
    enum {
        ex0 = 0,
        ex1,
        ex2,
    }v_enum;
    char v_char;
    unsigned char v_uchar;
    short v_short;
    unsigned short v_ushort;
    int v_int;
    unsigned int v_uint;
    long v_long;
    unsigned long v_ulong;
    long long v_llong;
    unsigned long long v_ullong;
    bool v_bool;
    float v_float;
    double v_double;
    long double v_ldouble;
    float complex v_float_complex;
    double complex v_double_complex;
    long double complex v_ldouble_complex;
    struct _test_metac_value_equal_ex_1 * v_ex1;
} empty_ex1_1, empty_ex1_2, empty_ex1_3 = {.v_bool = true};

#define EX1_VAL { \
    .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555, \
    .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true, \
    .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05, \
    .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1, \
    .v_ex1 = NULL, \
    }
#define EX1_SETTINGS \
    .mode = (metac_value_memory_map_mode_t){.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_fail}, \
    .p_tag_map = NULL

METAC_START_TEST(test_metac_value_equal_ex_1) {
    struct {
        struct _test_metac_value_equal_ex_1 * p_in1;
        struct _test_metac_value_equal_ex_1 * p_in2;
        metac_value_memory_map_mode_t mode;
        metac_tag_map_t *p_tag_map;
        int expected_out;
    }tcs[] = {
        {
            .p_in1 = NULL,
            .p_in2 = NULL,
            EX1_SETTINGS,
            .expected_out = 1,
        },
        {
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = NULL,
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {
            .p_in1 = NULL,
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            EX1_SETTINGS,
            .expected_out = 1,
        },
        {    /*changing values in 1 field */
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex2, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 2*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -44, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 3*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 55, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 4*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -346, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 5*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 345, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 6*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11112, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 7*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55556,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 8*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -445, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 9*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 445, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 10*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111112, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 11*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 6, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 12*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = false,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 13*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.2, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 14*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.02, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 15*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.06,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 16*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.3*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 17*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -0.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 18*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 3.1,
                .v_ex1 = NULL,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* fld 19*/
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){EX1_VAL},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = &empty_ex1_1,
            }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
        {   /* compare values in pointers */
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = &empty_ex1_1,
            }},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ 
                .v_enum = ex1, .v_char = -43, .v_uchar = 54, .v_short = -345, .v_ushort = 344, .v_int = -11111, .v_uint = 55555,
                .v_long = -444, .v_ulong = 444, .v_llong = -111111, .v_ullong = 5, .v_bool = true,
                .v_float = -0.1, .v_double = -0.01, .v_ldouble = 0.05,
                .v_float_complex = 1.2*I + 2.1, .v_double_complex = -1.2*I + 2.1, .v_ldouble_complex = -1.2*I - 2.1,
                .v_ex1 = &empty_ex1_1,
            }},
            EX1_SETTINGS,
            .expected_out = 1,
        },
        {   /* compare values in pointers */
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){{.v_ex1 = &empty_ex1_1}},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{.v_ex1 = &empty_ex1_2}},
            EX1_SETTINGS,
            .expected_out = 1,
        },
        {   /* compare values in pointers */
            .p_in1 = (struct _test_metac_value_equal_ex_1[]){{ .v_ex1 = &empty_ex1_1 }},
            .p_in2 = (struct _test_metac_value_equal_ex_1[]){{ .v_ex1 = &empty_ex1_3 }},
            EX1_SETTINGS,
            .expected_out = 0,
        },
    };
    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        WITH_METAC_DECLLOC(loc, struct _test_metac_value_equal_ex_1 * p_in1 = tcs[tc_inx].p_in1, * p_in2 = tcs[tc_inx].p_in2);
        metac_value_t *p_val1 = METAC_VALUE_FROM_DECLLOC(loc, p_in1);
        metac_value_t *p_val2 = METAC_VALUE_FROM_DECLLOC(loc, p_in2);

        int out = metac_value_equal_ex(p_val1, p_val2, &tcs[tc_inx].mode, tcs[tc_inx].p_tag_map);

        fail_unless(out == tcs[tc_inx].expected_out, "tc_inx %i: expected out %i got %i",
            tc_inx, tcs[tc_inx].expected_out, out);
        if (p_val1 != NULL) {
            metac_value_delete(p_val1);
        }
        if (p_val2 != NULL) {
            metac_value_delete(p_val2);
        }
    }
}END_TEST

METAC_START_TEST(test_metac_value_equal_with_t3) {
    metac_tag_map_t *p_tag_map = new_t3_tag_map();
    fail_unless(p_tag_map != NULL, "Couldn't create tagmap");

    WITH_METAC_DECLLOC(t3, struct t3 *p_t3 = _create_t3_test_data(), *p_t3_tree_cp = NULL, *p_t3_dag_cp = NULL);
    metac_value_t *t3_val = METAC_VALUE_FROM_DECLLOC(t3, p_t3);
    fail_unless(t3_val != NULL, "got t3_val NULL value");

    metac_value_t *t3_tree_cp_val = METAC_VALUE_FROM_DECLLOC(t3, p_t3_tree_cp);
    fail_unless(t3_tree_cp_val != NULL, "got p_t3_tree_cp NULL value");
    metac_value_t *t3_dag_cp_val = METAC_VALUE_FROM_DECLLOC(t3, p_t3_dag_cp);
    fail_unless(t3_dag_cp_val != NULL, "got t3_dag_cp_val NULL value");

    fail_unless(metac_value_copy_ex(t3_val, t3_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree}},
        NULL, NULL,
        p_tag_map) != NULL, "p_t3_tree_cp failed");
    fail_unless(p_t3_tree_cp != NULL, "metac_value_copy_ex p_t3_tree_cp failed");

    fail_unless(metac_value_copy_ex(t3_val, t3_dag_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag}},
        NULL, NULL,
        p_tag_map) != NULL, "t3_dag_cp_val failed");
    fail_unless(p_t3_tree_cp != NULL, "metac_value_copy_ex t3_dag_cp_val failed");

    fail_unless(metac_value_equal_ex(t3_val, t3_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree}},
        p_tag_map) != 0, "expected equal true for t3_val, t3_tree_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t3_val, t3_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag}},
        p_tag_map) == 0, "expected equal false for t3_val, t3_tree_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t3_val, t3_dag_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree}},
        p_tag_map) != 0, "expected equal true for t3_val, t3_dag_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t3_val, t3_dag_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag}},
        p_tag_map) != 0, "expected equal true for t3_val, t3_dag_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t3_dag_cp_val, t3_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree}},
        p_tag_map) != 0, "expected equal true for t3_dag_cp_val, t3_tree_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t3_dag_cp_val, t3_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag}},
        p_tag_map) == 0, "expected equal false for t3_dag_cp_val, t3_tree_cp_val in tree mode");


    fail_unless(metac_value_free_ex(t3_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, NULL, p_tag_map) != 0, "metac_value_free_ex t3_tree_cp_val failed");
    fail_unless(metac_value_free_ex(t3_dag_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, NULL, p_tag_map) != 0, "metac_value_free_ex t3_dag_cp_val failed");
    fail_unless(metac_value_free_ex(t3_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, NULL, p_tag_map) != 0, "metac_value_free_ex t3_val failed");
    metac_value_delete(t3_tree_cp_val);
    metac_value_delete(t3_dag_cp_val);
    metac_value_delete(t3_val);

    metac_tag_map_delete(p_tag_map);
}END_TEST

/* containerof case . see https://en.wikipedia.org/wiki/Offsetof */
struct list_entry {
    struct list_entry * p_next;
};
struct t4 {
    int data;
    struct list_entry lst;
};

/* option with variable tags inplace */
METAC_TAG_MAP_NEW(new_module_tag_map, NULL, {.mask =
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_subprogram_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)

    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct list_entry) 
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n= "p_next"}),
            METAC_CONTAINER_OF(struct t4, lst)
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END

/* option with final enty (not scalable) */
METAC_TAG_MAP_NEW(new_module_tag_map_with_final, NULL, {.mask =
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_subprogram_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)

    METAC_TAG_MAP_ENTRY_FROM_TYPE(struct list_entry)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_final_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n= "p_next"}),
            METAC_CONTAINER_OF(struct t4, lst)
        )
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END

static void *dcalloc(size_t nmemb, size_t size) {
    void * ptr = calloc(nmemb, size);
    //printf("calloc %li %li => %p\n", nmemb, size, ptr);
    return ptr;
}

static void dfree(void *ptr) {
    //printf("free %p\n", ptr);
    free(ptr);
}

struct list_entry * _create_t4_test_data() {
    struct t4 * x[5];
    for (size_t i = 0; i < sizeof(x)/sizeof(x[0]); ++i) {
        x[i] = dcalloc(1, sizeof(*x[i]));
        fail_unless(x[i] != NULL, "failed _create_t4_test_data to allocate mem");
        x[i]->data = 10 + i;
        x[i]->lst.p_next = NULL;
        if (i>0) {
            x[i-1]->lst.p_next = &(x[i]->lst);
        }
    }
    return &x[0]->lst;
}

static void test_metac_value_fns_with_t4_with_tags(metac_flag_t use_final_entry) {
    metac_tag_map_t *p_tag_map = NULL;

    char *s, *expected_s;
    WITH_METAC_DECLLOC(loc, struct list_entry * p_t4 = _create_t4_test_data(), *p_t4_tree_cp = NULL, *p_t4_dag_cp = NULL);
    metac_value_t *t4_val = METAC_VALUE_FROM_DECLLOC(loc, p_t4);
    fail_unless(t4_val != NULL, "got t4_val NULL value");
    
    expected_s = "(struct list_entry []){{.p_next = (struct list_entry []){{.p_next = (struct list_entry []){{.p_next = (struct list_entry []){{.p_next = (struct list_entry []){{.p_next = NULL,},},},},},},},},},}";
    s = metac_value_string_ex(t4_val, METAC_WMODE_deep, NULL);
    fail_unless(s != NULL, "got null string for t4_val");
    fail_unless(strcmp(expected_s, s) == 0, "expected %s got %s", expected_s, s);
    free(s);

    metac_value_t *t4_tree_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t4_tree_cp);
    fail_unless(t4_tree_cp_val != NULL, "got p_t4_tree_cp NULL value");
    metac_value_t *t4_dag_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t4_dag_cp);
    fail_unless(t4_dag_cp_val != NULL, "got t4_dag_cp_val NULL value");

    if (use_final_entry == 0) {
        p_tag_map = new_module_tag_map();
        fail_unless(p_tag_map != NULL, "Couldn't create tagmap");

        fail_unless(metac_tag_map_set_tag(p_tag_map, 0, 0, metac_value_entry(t4_val), METAC_TEO_entry,
                METAC_TAG(METAC_CONTAINER_OF(struct t4, lst))) == 0, "metac_tag_map_set_tag 1 failed");
        fail_unless(metac_tag_map_set_tag(p_tag_map, 0, 0, metac_value_entry(t4_tree_cp_val), METAC_TEO_entry,
                METAC_TAG(METAC_CONTAINER_OF(struct t4, lst))) == 0, "metac_tag_map_set_tag 2 failed");
        fail_unless(metac_tag_map_set_tag(p_tag_map, 0, 0, metac_value_entry(t4_dag_cp_val), METAC_TEO_entry,
                METAC_TAG(METAC_CONTAINER_OF(struct t4, lst))) == 0, "metac_tag_map_set_tag 3 failed");
    }else {
        p_tag_map = new_module_tag_map_with_final(); 
        fail_unless(p_tag_map != NULL, "Couldn't create tagmap");
    }

    fail_unless(metac_value_copy_ex(t4_val, t4_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree}},
        dcalloc, dfree,
        p_tag_map) != NULL, "p_t4_tree_cp failed");
    fail_unless(p_t4_tree_cp != NULL, "metac_value_copy_ex p_t4_tree_cp failed");

    s = metac_value_string_ex(t4_tree_cp_val, METAC_WMODE_deep, NULL);
    fail_unless(s != NULL, "got null string for t4_val");
    fail_unless(strcmp(expected_s, s) == 0, "expected %s got %s", expected_s, s);
    free(s);

    fail_unless(metac_value_copy_ex(t4_val, t4_dag_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag}},
        dcalloc, dfree,
        p_tag_map) != NULL, "t4_dag_cp_val failed");
    fail_unless(p_t4_tree_cp != NULL, "metac_value_copy_ex t4_dag_cp_val failed");

    s = metac_value_string_ex(t4_dag_cp_val, METAC_WMODE_deep, NULL);
    fail_unless(s != NULL, "got null string for t4_val");
    fail_unless(strcmp(expected_s, s) == 0, "expected %s got %s", expected_s, s);
    free(s);

    fail_unless(metac_value_equal_ex(t4_val, t4_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree}},
        p_tag_map) != 0, "expected equal true for t4_val, t4_tree_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t4_val, t4_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag}},
        p_tag_map) != 0, "expected equal false for t4_val, t4_tree_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t4_val, t4_dag_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree}},
        p_tag_map) != 0, "expected equal true for t4_val, t4_dag_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t4_val, t4_dag_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag}},
        p_tag_map) != 0, "expected equal true for t4_val, t4_dag_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t4_dag_cp_val, t4_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree}},
        p_tag_map) != 0, "expected equal true for t4_dag_cp_val, t4_tree_cp_val in tree mode");

    fail_unless(metac_value_equal_ex(t4_dag_cp_val, t4_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag}},
        p_tag_map) != 0, "expected equal false for t4_dag_cp_val, t4_tree_cp_val in tree mode");


    fail_unless(metac_value_free_ex(t4_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, dfree, p_tag_map) != 0, "metac_value_free_ex t4_tree_cp_val failed");
    fail_unless(metac_value_free_ex(t4_dag_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, dfree, p_tag_map) != 0, "metac_value_free_ex t4_dag_cp_val failed");
    fail_unless(metac_value_free_ex(t4_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_fail,}}, dfree, p_tag_map) != 0, "failed to free t4_val");

    metac_value_delete(t4_tree_cp_val);
    metac_value_delete(t4_dag_cp_val);
    metac_value_delete(t4_val);

    metac_tag_map_delete(p_tag_map);
}

METAC_START_TEST(test_metac_value_fns_with_t4) {
    test_metac_value_fns_with_t4_with_tags(0);
    test_metac_value_fns_with_t4_with_tags(1);
}END_TEST

struct t5 {
    int x;
    void *p;
};
struct t5 * _create_t5_test_data() {
    struct t5 * res = tcalloc(1, sizeof(*res));
    fail_unless(res != NULL, "can't allocate t5 mem");
    res->x = 100;
    res->p = calloc(1,1);
    fail_unless(res->p != NULL, "can't allocate t5->p mem");
    return res;
}

METAC_START_TEST(test_metac_value_fns_with_t5) {
    WITH_METAC_DECLLOC(loc, struct t5 * p_t5 = _create_t5_test_data(), *p_t5_tree_cp = NULL, *p_t5_dag_cp = NULL);
    metac_value_t *t5_val = METAC_VALUE_FROM_DECLLOC(loc, p_t5);
    fail_unless(t5_val != NULL, "got t4_val NULL value");
    metac_value_t *t5_tree_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t5_tree_cp);
    fail_unless(t5_tree_cp_val != NULL, "got p_t5_tree_cp NULL value");
    metac_value_t *t5_dag_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t5_dag_cp);
    fail_unless(t5_dag_cp_val != NULL, "got t5_dag_cp_val NULL value");

    /* copy in tree mode */
    fail_unless(metac_value_copy_ex(t5_val, t5_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_fail}},
        tcalloc, tfree,
        NULL) == NULL, "p_t5_tree_cp expected to be failed");
    fail_unless(p_t5_tree_cp == NULL, "metac_value_copy_ex p_t5_tree_cp expected to be NULL");

    fail_unless(metac_value_copy_ex(t5_val, t5_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_ignore}},
        tcalloc, tfree,
        NULL) != NULL, "p_t5_tree_cp expected to pass");
    fail_unless(p_t5_tree_cp != NULL, "metac_value_copy_ex p_t5_tree_cp expected to be not NULL");
    fail_unless(p_t5_tree_cp->p == NULL, "metac_value_copy_ex p_t5_tree_cp->p expected to be NULL, got %p", p_t5_tree_cp->p);
    fail_unless(metac_value_equal_ex(t5_val, t5_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_ignore}},
        NULL) != 0, "expected equal true for t5_val, t5_tree_cp_val in tree mode");
    fail_unless(metac_value_equal_ex(t5_val, t5_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_as_values}},
        NULL) == 0, "expected equal false for t5_val, t5_tree_cp_val in tree mode");
    fail_unless(metac_value_free_ex(t5_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, tfree, NULL) != 0, "failed to free t5_val");

    fail_unless(metac_value_copy_ex(t5_val, t5_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_as_values}},
        tcalloc, tfree,
        NULL) != NULL, "p_t5_tree_cp expected to pass");
    fail_unless(p_t5_tree_cp != NULL, "metac_value_copy_ex p_t5_tree_cp expected to be not NULL");
    fail_unless(p_t5_tree_cp->p == p_t5->p, "metac_value_copy_ex p_t5_tree_cp->p expected to be p_t5->p, got %p", p_t5_tree_cp->p);
    fail_unless(metac_value_equal_ex(t5_val, t5_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_ignore}},
        NULL) != 0, "expected equal true for t5_val, t5_tree_cp_val in tree mode");
    fail_unless(metac_value_equal_ex(t5_val, t5_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_as_values}},
        NULL) != 0, "expected equal false for t5_val, t5_tree_cp_val in tree mode");
    fail_unless(metac_value_free_ex(t5_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_as_values,}}, tfree, NULL) != 0, "failed to free t5_val");

    // DAG mode must work the same way
    p_t5_tree_cp = NULL;
    fail_unless(metac_value_copy_ex(t5_val, t5_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_fail}},
        tcalloc, tfree,
        NULL) == NULL, "p_t5_tree_cp expected to be failed");
    fail_unless(p_t5_tree_cp == NULL, "metac_value_copy_ex p_t5_tree_cp expected to be NULL");

    fail_unless(metac_value_copy_ex(t5_val, t5_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_ignore}},
        tcalloc, tfree,
        NULL) != NULL, "p_t5_tree_cp expected to pass");
    fail_unless(p_t5_tree_cp != NULL, "metac_value_copy_ex p_t5_tree_cp expected to be not NULL");
    fail_unless(p_t5_tree_cp->p == NULL, "metac_value_copy_ex p_t5_tree_cp->p expected to be NULL, got %p", p_t5_tree_cp->p);
    fail_unless(metac_value_equal_ex(t5_val, t5_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_ignore}},
        NULL) != 0, "expected equal true for t5_val, t5_tree_cp_val in tree mode");
    fail_unless(metac_value_equal_ex(t5_val, t5_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_as_values}},
        NULL) == 0, "expected equal false for t5_val, t5_tree_cp_val in tree mode");
    fail_unless(metac_value_free_ex(t5_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, tfree, NULL) != 0, "failed to free t5_val");

    fail_unless(metac_value_copy_ex(t5_val, t5_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_as_values}},
        tcalloc, tfree,
        NULL) != NULL, "p_t5_tree_cp expected to pass");
    fail_unless(p_t5_tree_cp != NULL, "metac_value_copy_ex p_t5_tree_cp expected to be not NULL");
    fail_unless(p_t5_tree_cp->p == p_t5->p, "metac_value_copy_ex p_t5_tree_cp->p expected to be p_t5->p, got %p", p_t5_tree_cp->p);
    fail_unless(metac_value_equal_ex(t5_val, t5_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_ignore}},
        NULL) != 0, "expected equal true for t5_val, t5_tree_cp_val in tree mode");
    fail_unless(metac_value_equal_ex(t5_val, t5_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_as_values}},
        NULL) != 0, "expected equal false for t5_val, t5_tree_cp_val in tree mode");
    fail_unless(metac_value_free_ex(t5_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_as_values,}}, tfree, NULL) != 0, "failed to free t5_val");

    /* test free */
    fail_unless(metac_value_free_ex(t5_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_fail, }}, tfree, NULL) == 0, "expected fail to free t5_val");
    void *p = p_t5->p;// KEEP ptr so all memory is cleaned
    fail_unless(metac_value_free_ex(t5_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, tfree, NULL) != 0, "failed to free t5_val");
    tfree(p);

    p_t5 = _create_t5_test_data();
    fail_unless(metac_value_free_ex(t5_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_as_one_byte_ptr,}}, tfree, NULL) != 0, "failed to free t5_val");

    metac_value_delete(t5_tree_cp_val);
    metac_value_delete(t5_dag_cp_val);
    metac_value_delete(t5_val);
}END_TEST

/* the same, but use some declaration type instead of void + enum to test enum copy*/
typedef enum {
    t6e_0 = 0,
    t6e_1,
    t6e_2,
}t6_enum;
struct t6 {
    int x;
    t6_enum e;
    struct ___undeclared__ *p;
};
struct t6 * _create_t6_test_data() {
    struct t6 * res = tcalloc(1, sizeof(*res));
    fail_unless(res != NULL, "can't allocate t6 mem");
    res->x = 100;
    res->e = t6e_1;
    res->p = calloc(1,1);
    fail_unless(res->p != NULL, "can't allocate t6->p mem");
    return res;
}

METAC_START_TEST(test_metac_value_fns_with_t6) {
    WITH_METAC_DECLLOC(loc, struct t6 * p_t6 = _create_t6_test_data(), *p_t6_tree_cp = NULL, *p_t6_dag_cp = NULL);
    metac_value_t *t6_val = METAC_VALUE_FROM_DECLLOC(loc, p_t6);
    fail_unless(t6_val != NULL, "got t6_val NULL value");
    metac_value_t *t6_tree_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t6_tree_cp);
    fail_unless(t6_tree_cp_val != NULL, "got p_t6_tree_cp NULL value");
    metac_value_t *t6_dag_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t6_dag_cp);
    fail_unless(t6_dag_cp_val != NULL, "got t6_dag_cp_val NULL value");

    /* copy in tree mode */
    fail_unless(metac_value_copy_ex(t6_val, t6_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_fail}},
        tcalloc, tfree,
        NULL) == NULL, "p_t6_tree_cp expected to be failed");
    fail_unless(p_t6_tree_cp == NULL, "metac_value_copy_ex p_t6_tree_cp expected to be NULL");

    fail_unless(metac_value_copy_ex(t6_val, t6_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_ignore}},
        tcalloc, tfree,
        NULL) != NULL, "p_t6_tree_cp expected to pass");
    fail_unless(p_t6_tree_cp != NULL, "metac_value_copy_ex p_t6_tree_cp expected to be not NULL");
    fail_unless(p_t6_tree_cp->x == p_t6->x, "metac_value_copy_ex p_t6_tree_cp expected to have the same x field");
    fail_unless(p_t6_tree_cp->e == p_t6->e, "metac_value_copy_ex p_t6_tree_cp expected to have the same e field");
    fail_unless(p_t6_tree_cp->p == NULL, "metac_value_copy_ex p_t6_tree_cp->p expected to be NULL, got %p", p_t6_tree_cp->p);
    fail_unless(metac_value_equal_ex(t6_val, t6_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_ignore}},
        NULL) != 0, "expected equal true for t6_val, t6_tree_cp_val in tree mode");
    fail_unless(metac_value_equal_ex(t6_val, t6_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_as_values}},
        NULL) == 0, "expected equal false for t6_val, t6_tree_cp_val in tree mode");
    fail_unless(metac_value_free_ex(t6_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, tfree, NULL) != 0, "failed to free t6_val");

    fail_unless(metac_value_copy_ex(t6_val, t6_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_as_values}},
        tcalloc, tfree,
        NULL) != NULL, "p_t6_tree_cp expected to pass");
    fail_unless(p_t6_tree_cp != NULL, "metac_value_copy_ex p_t6_tree_cp expected to be not NULL");
    fail_unless(p_t6_tree_cp->x == p_t6->x, "metac_value_copy_ex p_t6_tree_cp expected to have the same x field");
    fail_unless(p_t6_tree_cp->e == p_t6->e, "metac_value_copy_ex p_t6_tree_cp expected to have the same e field");
    fail_unless(p_t6_tree_cp->p == p_t6->p, "metac_value_copy_ex p_t6_tree_cp->p expected to be p_t6->p, got %p", p_t6_tree_cp->p);
    fail_unless(metac_value_equal_ex(t6_val, t6_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_ignore}},
        NULL) != 0, "expected equal true for t6_val, t6_tree_cp_val in tree mode");
    fail_unless(metac_value_equal_ex(t6_val, t6_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .unknown_ptr_mode = METAC_UPTR_as_values}},
        NULL) != 0, "expected equal false for t6_val, t6_tree_cp_val in tree mode");
    fail_unless(metac_value_free_ex(t6_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_as_values,}}, tfree, NULL) != 0, "failed to free t6_val");

    // DAG mode must work the same way
    p_t6_tree_cp = NULL;
    fail_unless(metac_value_copy_ex(t6_val, t6_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_fail}},
        tcalloc, tfree,
        NULL) == NULL, "p_t6_tree_cp expected to be failed");
    fail_unless(p_t6_tree_cp == NULL, "metac_value_copy_ex p_t6_tree_cp expected to be NULL");

    fail_unless(metac_value_copy_ex(t6_val, t6_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_ignore}},
        tcalloc, tfree,
        NULL) != NULL, "p_t6_tree_cp expected to pass");
    fail_unless(p_t6_tree_cp != NULL, "metac_value_copy_ex p_t6_tree_cp expected to be not NULL");
    fail_unless(p_t6_tree_cp->p == NULL, "metac_value_copy_ex p_t6_tree_cp->p expected to be NULL, got %p", p_t6_tree_cp->p);
    fail_unless(metac_value_equal_ex(t6_val, t6_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_ignore}},
        NULL) != 0, "expected equal true for t6_val, t6_tree_cp_val in tree mode");
    fail_unless(metac_value_equal_ex(t6_val, t6_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_as_values}},
        NULL) == 0, "expected equal false for t6_val, t6_tree_cp_val in tree mode");
    fail_unless(metac_value_free_ex(t6_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, tfree, NULL) != 0, "failed to free t6_val");

    fail_unless(metac_value_copy_ex(t6_val, t6_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_as_values}},
        tcalloc, tfree,
        NULL) != NULL, "p_t6_tree_cp expected to pass");
    fail_unless(p_t6_tree_cp != NULL, "metac_value_copy_ex p_t6_tree_cp expected to be not NULL");
    fail_unless(p_t6_tree_cp->p == p_t6->p, "metac_value_copy_ex p_t6_tree_cp->p expected to be p_t6->p, got %p", p_t6_tree_cp->p);
    fail_unless(metac_value_equal_ex(t6_val, t6_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_ignore}},
        NULL) != 0, "expected equal true for t6_val, t6_tree_cp_val in tree mode");
    fail_unless(metac_value_equal_ex(t6_val, t6_tree_cp_val, 
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .unknown_ptr_mode = METAC_UPTR_as_values}},
        NULL) != 0, "expected equal false for t6_val, t6_tree_cp_val in tree mode");
    fail_unless(metac_value_free_ex(t6_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_as_values, }}, tfree, NULL) != 0, "failed to free t6_val");

    /* test free */
    fail_unless(metac_value_free_ex(t6_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_fail, }}, tfree, NULL) == 0, "expected fail to free t6_val");
    void *p = p_t6->p;// KEEP ptr so all memory is cleaned
    fail_unless(metac_value_free_ex(t6_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_ignore,}}, tfree, NULL) != 0, "failed to free t6_val");
    tfree(p);

    p_t6 = _create_t6_test_data();
    fail_unless(metac_value_free_ex(t6_val, (metac_value_memory_map_non_handled_mode_t[]){{.unknown_ptr_mode = METAC_UPTR_as_one_byte_ptr,}}, tfree, NULL) != 0, "failed to free t6_val");

    metac_value_delete(t6_tree_cp_val);
    metac_value_delete(t6_dag_cp_val);
    metac_value_delete(t6_val);
}END_TEST

struct t7 {
    int x;
    union {
        int i;
        unsigned int u;
    };
};
struct t7 * _create_t7_test_data() {
    struct t7 * res = tcalloc(1, sizeof(*res));
    fail_unless(res != NULL, "can't allocate t7 mem");
    res->x = 1;
    res->i = 1;
    return res;
}

METAC_START_TEST(test_metac_value_fns_with_t7) {
    WITH_METAC_DECLLOC(loc, struct t7 * p_t7 = _create_t7_test_data(), *p_t7_tree_cp = NULL, *p_t7_dag_cp = NULL);
    metac_value_t *t7_val = METAC_VALUE_FROM_DECLLOC(loc, p_t7);
    fail_unless(t7_val != NULL, "got t7_val NULL value");
    metac_value_t *t7_tree_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t7_tree_cp);
    fail_unless(t7_tree_cp_val != NULL, "got p_t7_tree_cp NULL value");
    metac_value_t *t7_dag_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t7_dag_cp);
    fail_unless(t7_dag_cp_val != NULL, "got t7_dag_cp_val NULL value");

    /* copy in tree mode */
    fail_unless(metac_value_copy_ex(t7_val, t7_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .union_mode = METAC_UNION_fail}},
        tcalloc, tfree,
        NULL) == NULL, "p_t7_tree_cp expected to be failed");
    fail_unless(p_t7_tree_cp == NULL, "metac_value_copy_ex p_t7_tree_cp expected to be NULL");

    fail_unless(metac_value_copy_ex(t7_val, t7_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .union_mode = METAC_UNION_ignore}},
        tcalloc, tfree,
        NULL) != NULL, "p_t7_tree_cp is failed");
    fail_unless(p_t7_tree_cp != NULL, "metac_value_copy_ex p_t7_tree_cp expected to be NULL");
    fail_unless(p_t7_tree_cp->i == 0, "metac_value_copy_ex p_t7_tree_cp->i expected ==0, got %i", p_t7_tree_cp->i);
    fail_unless(metac_value_free_ex(t7_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.union_mode = METAC_UNION_ignore,}}, tfree, NULL) != 0,
        "failed free");

    fail_unless(metac_value_copy_ex(t7_val, t7_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .union_mode = METAC_UNION_as_mem}},
        tcalloc, tfree,
        NULL) != NULL, "p_t7_tree_cp is failed");
    fail_unless(p_t7_tree_cp != NULL, "metac_value_copy_ex p_t7_tree_cp expected to be NULL");
    fail_unless(p_t7_tree_cp->i == 1, "metac_value_copy_ex p_t7_tree_cp->i expected ==1, got %i", p_t7_tree_cp->i);
    fail_unless(metac_value_free_ex(t7_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.union_mode = METAC_UNION_as_mem,}}, tfree, NULL) != 0,
        "failed free");

    /* copy in dag mode */
    fail_unless(metac_value_copy_ex(t7_val, t7_dag_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .union_mode = METAC_UNION_fail}},
        tcalloc, tfree,
        NULL) == NULL, "p_t7_dag_cp expected to be failed");
    fail_unless(p_t7_dag_cp == NULL, "metac_value_copy_ex p_t7_dag_cp expected to be NULL");

    fail_unless(metac_value_copy_ex(t7_val, t7_dag_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .union_mode = METAC_UNION_ignore}},
        tcalloc, tfree,
        NULL) != NULL, "p_t7_dag_cp is failed");
    fail_unless(p_t7_dag_cp != NULL, "metac_value_copy_ex p_t7_dag_cp expected to be NULL");
    fail_unless(p_t7_dag_cp->i == 0, "metac_value_copy_ex p_t7_dag_cp->i expected == 0, got %i", p_t7_dag_cp->i);
    fail_unless(metac_value_free_ex(t7_dag_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.union_mode = METAC_UNION_ignore,}}, tfree, NULL) != 0,
        "failed free");

    fail_unless(metac_value_copy_ex(t7_val, t7_dag_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .union_mode = METAC_UNION_as_mem}},
        tcalloc, tfree,
        NULL) != NULL, "p_t7_dag_cp is failed");
    fail_unless(p_t7_dag_cp != NULL, "metac_value_copy_ex p_t7_dag_cp expected to be NULL");
    fail_unless(p_t7_dag_cp->i == 1, "metac_value_copy_ex p_t7_dag_cp->i expected ==1, got %i", p_t7_dag_cp->i);
    fail_unless(metac_value_free_ex(t7_dag_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.union_mode = METAC_UNION_as_mem,}}, tfree, NULL) != 0,
        "failed free");


    // test free
    fail_unless(metac_value_free_ex(t7_val, (metac_value_memory_map_non_handled_mode_t[]){{.union_mode = METAC_UNION_fail,}}, tfree, NULL) == 0,
        "must fail");
    fail_unless(metac_value_free_ex(t7_val, (metac_value_memory_map_non_handled_mode_t[]){{.union_mode = METAC_UNION_ignore,}}, tfree, NULL) != 0,
        "failed free");
    p_t7 = _create_t7_test_data();
    fail_unless(metac_value_free_ex(t7_val, (metac_value_memory_map_non_handled_mode_t[]){{.union_mode = METAC_UNION_as_mem,}}, tfree, NULL) != 0,
        "failed free");

    metac_value_delete(t7_tree_cp_val);
    metac_value_delete(t7_dag_cp_val);
    metac_value_delete(t7_val);
}END_TEST

struct t8 {
    int l;
    int a[];
};

struct t8 * _create_t8_test_data() {
    struct t8 * res = tcalloc(1, offsetof(struct t8, a[5]));
    fail_unless(res != NULL, "can't allocate t8 mem");
    res->l = 5;
    return res;
}

static size_t t8_calloc_last_nmemb = 0;
static size_t t8_calloc_last_size = 0;
static void *t8calloc(size_t nmemb, size_t size) {
    t8_calloc_last_nmemb = nmemb;
    t8_calloc_last_size = size;

    return tcalloc(nmemb, size);
}

METAC_START_TEST(test_metac_value_fns_with_t8) {
    WITH_METAC_DECLLOC(loc, struct t8 * p_t8 = _create_t8_test_data(), *p_t8_tree_cp = NULL, *p_t8_dag_cp = NULL);
    metac_value_t *t8_val = METAC_VALUE_FROM_DECLLOC(loc, p_t8);
    fail_unless(t8_val != NULL, "got t8_val NULL value");
    metac_value_t *t8_tree_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t8_tree_cp);
    fail_unless(t8_tree_cp_val != NULL, "got p_t8_tree_cp NULL value");
    metac_value_t *t8_dag_cp_val = METAC_VALUE_FROM_DECLLOC(loc, p_t8_dag_cp);
    fail_unless(t8_dag_cp_val != NULL, "got t8_dag_cp_val NULL value");

    /* copy in tree mode */
    fail_unless(metac_value_copy_ex(t8_val, t8_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .flex_array_mode = METAC_FLXARR_fail}},
        tcalloc, tfree,
        NULL) == NULL, "p_t8_tree_cp expected to be failed");
    fail_unless(p_t8_tree_cp == NULL, "metac_value_copy_ex p_t8_tree_cp expected to be NULL");

    t8_calloc_last_nmemb = 0;
    t8_calloc_last_size = 0; 
    fail_unless(metac_value_copy_ex(t8_val, t8_tree_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_tree, .flex_array_mode = METAC_FLXARR_ignore}},
        t8calloc, tfree,
        NULL) != NULL, "p_t8_tree_cp is failed");
    fail_unless(p_t8_tree_cp != NULL, "metac_value_copy_ex p_t8_tree_cp expected to be NULL");
    fail_unless(t8_calloc_last_nmemb * t8_calloc_last_size == sizeof(struct t8), "expected allocated sz %li, got %li",
        sizeof(struct t8),
        t8_calloc_last_nmemb * t8_calloc_last_size);
    fail_unless(metac_value_free_ex(t8_tree_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.flex_array_mode = METAC_FLXARR_ignore,}}, tfree, NULL) != 0,
        "failed free");

    /* copy in dag mode */
    fail_unless(metac_value_copy_ex(t8_val, t8_dag_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .flex_array_mode = METAC_FLXARR_fail}},
        tcalloc, tfree,
        NULL) == NULL, "p_t8_dag_cp expected to be failed");
    fail_unless(p_t8_dag_cp == NULL, "metac_value_copy_ex p_t8_dag_cp expected to be NULL");

    t8_calloc_last_nmemb = 0;
    t8_calloc_last_size = 0; 
    fail_unless(metac_value_copy_ex(t8_val, t8_dag_cp_val,
        (metac_value_memory_map_mode_t[]){{.memory_block_mode = METAC_MMODE_dag, .flex_array_mode = METAC_FLXARR_ignore}},
        t8calloc, tfree,
        NULL) != NULL, "p_t8_dag_cp is failed");
    fail_unless(p_t8_dag_cp != NULL, "metac_value_copy_ex p_t8_dag_cp expected to be NULL");
    fail_unless(t8_calloc_last_nmemb * t8_calloc_last_size == sizeof(struct t8), "expected allocated sz %li, got %li",
        sizeof(struct t8),
        t8_calloc_last_nmemb * t8_calloc_last_size);
    fail_unless(metac_value_free_ex(t8_dag_cp_val, (metac_value_memory_map_non_handled_mode_t[]){{.flex_array_mode = METAC_FLXARR_ignore,}}, tfree, NULL) != 0,
        "failed free");

    // test free
    fail_unless(metac_value_free_ex(t8_val, (metac_value_memory_map_non_handled_mode_t[]){{.flex_array_mode = METAC_FLXARR_fail,}}, tfree, NULL) == 0,
        "must fail");
    fail_unless(metac_value_free_ex(t8_val, (metac_value_memory_map_non_handled_mode_t[]){{.flex_array_mode = METAC_FLXARR_ignore,}}, tfree, NULL) != 0,
        "failed free");

    metac_value_delete(t8_tree_cp_val);
    metac_value_delete(t8_dag_cp_val);
    metac_value_delete(t8_val);
}END_TEST

typedef struct {
    bool is_ptr;
    union {
        int val;
        int *p_val;
    };
}test9_t;

typedef struct {
	float f;
    double d;
    long double ld;
    float complex fc;
    double complex dc;
    long double complex ldc;

    int len;
    char flex[];
}test10_t;


METAC_TAG_MAP_NEW(t9_10_tag_map, NULL, {.mask =
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_subprogram_parameter) | 
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member) |
            METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final),},)
    METAC_TAG_MAP_ENTRY_FROM_TYPE(test9_t)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.i=1}),
            METAC_UNION_MEMBER_SELECT_BY(is_ptr, 
                {.fld_val = false, .union_fld_name = "val"},
                {.fld_val = true, .union_fld_name = "p_val"}, 
            ))
    METAC_TAG_MAP_ENTRY_END

    METAC_TAG_MAP_ENTRY_FROM_TYPE(test10_t)
        METAC_TAG_MAP_SET_TAG(0, METAC_TEO_entry, 0, METAC_TAG_MAP_ENTRY_MEMBER({.n="flex"}), METAC_COUNT_BY(len))
    METAC_TAG_MAP_ENTRY_END

METAC_TAG_MAP_END

METAC_START_TEST(t9_test) {
    int some_value;
    metac_value_memory_map_mode_t mode = {};
    metac_value_memory_map_non_handled_mode_t nhmode = {};

    metac_tag_map_t *p_tag_map = t9_10_tag_map();
    fail_unless(p_tag_map != NULL, "tagmap is NULL");

    WITH_METAC_DECLLOC(loc, 
        test9_t * p_t9 = (test9_t[]){{.is_ptr = false, .val = 1 }};
        test9_t * p_t9_copy = NULL;
    )

    metac_value_t *p_t9_val = METAC_VALUE_FROM_DECLLOC(loc, p_t9);
    metac_value_t *p_t9_copy_val = METAC_VALUE_FROM_DECLLOC(loc, p_t9_copy);
    fail_unless(p_t9_val != NULL && p_t9_copy_val != NULL, "values are NULL");

    /*DAG mod*/
    mode.union_mode = METAC_UNION_as_mem;
    fail_unless(metac_value_copy_ex(p_t9_val, p_t9_copy_val, &mode, NULL, NULL, NULL)!= NULL, "memory mode copy failed");
    fail_unless(p_t9_copy != NULL, "copy didn't finish ok - copy is NULL");

    fail_unless(p_t9->is_ptr == p_t9_copy->is_ptr, "copy isn't equal to original");
    fail_unless(memcmp(&p_t9->val, &p_t9_copy->val, sizeof(p_t9->val))==0, "copy isn't equal to original");
 
 
    fail_unless(metac_value_equal_ex(p_t9_val, p_t9_copy_val, &mode, NULL) != 0, "equal fn has failed");

    some_value = 100;
    p_t9->p_val = &some_value;
    fail_unless(metac_value_equal_ex(p_t9_val, p_t9_copy_val, &mode, NULL) == 0, "equal fn has failed");
    p_t9->is_ptr = true;
    nhmode.union_mode = METAC_UNION_as_mem;
    fail_unless(metac_value_free_ex(p_t9_copy_val, &nhmode, free, NULL) != 0, "free_ex failed");

    // another test - now with tag map
    mode.union_mode = METAC_UNION_fail; // this will make it fail if tagmap isn't recoginzed
    nhmode.union_mode = METAC_UNION_fail;
    fail_unless(metac_value_copy_ex(p_t9_val, p_t9_copy_val, &mode, NULL, NULL, p_tag_map)!= NULL, "memory mode copy failed");
    fail_unless(p_t9_copy != NULL, "copy didn't finish ok - copy is NULL");
    fail_unless(p_t9->is_ptr == p_t9_copy->is_ptr &&
        p_t9_copy->p_val != NULL &&
        (*p_t9->p_val) == (*p_t9_copy->p_val), "memory didn't copy data correctly");

    fail_unless(metac_value_equal_ex(p_t9_val, p_t9_copy_val, &mode, p_tag_map) > 0, "equal fn has failed");
    some_value = 99;
    fail_unless(metac_value_equal_ex(p_t9_val, p_t9_copy_val, &mode, p_tag_map) == 0, "equal fn has failed");

    /* free copy */
    fail_unless(metac_value_free_ex(p_t9_copy_val, &nhmode, NULL, p_tag_map) != 0, "free_ex failed");

    /*TREE mod*/
    p_t9->is_ptr = false;
    p_t9->val = 1;
    mode.memory_block_mode = METAC_MMODE_tree;
    mode.union_mode = METAC_UNION_as_mem;
    fail_unless(metac_value_copy_ex(p_t9_val, p_t9_copy_val, &mode, NULL, NULL, NULL)!= NULL, "memory mode copy failed");
    fail_unless(p_t9_copy != NULL, "copy didn't finish ok - copy is NULL");
    
    fail_unless(p_t9->is_ptr == p_t9_copy->is_ptr, "copy isn't equal to original");
    fail_unless(memcmp(&p_t9->val, &p_t9_copy->val, sizeof(p_t9->val))==0, "copy isn't equal to original");
 
    fail_unless(metac_value_equal_ex(p_t9_val, p_t9_copy_val, &mode, NULL) != 0, "equal fn has failed");

    some_value = 100;
    p_t9->p_val = &some_value;
    fail_unless(metac_value_equal_ex(p_t9_val, p_t9_copy_val, &mode, NULL) == 0, "equal fn has failed");
    p_t9->is_ptr = true;
    nhmode.union_mode = METAC_UNION_as_mem;
    fail_unless(metac_value_free_ex(p_t9_copy_val, &nhmode, NULL, NULL) != 0, "free_ex failed");

    // another test - now with tag map
    mode.union_mode = METAC_UNION_fail; // this will make it fail if tagmap isn't recoginzed
    nhmode.union_mode = METAC_UNION_fail;
    fail_unless(metac_value_copy_ex(p_t9_val, p_t9_copy_val, &mode, NULL, NULL, NULL) == NULL , "memory mode copy not failed");
    fail_unless(metac_value_copy_ex(p_t9_val, p_t9_copy_val, &mode, NULL, NULL, p_tag_map)!= NULL, "memory mode copy failed");
    fail_unless(p_t9_copy != NULL, "copy didn't finish ok - copy is NULL");
    fail_unless(p_t9->is_ptr == p_t9_copy->is_ptr &&
        p_t9_copy->p_val != NULL &&
        (*p_t9->p_val) == (*p_t9_copy->p_val), "memory didn't copy data correctly");

    fail_unless(metac_value_equal_ex(p_t9_val, p_t9_copy_val, &mode, p_tag_map) > 0, "equal fn has failed");
    some_value = 99;
    fail_unless(metac_value_equal_ex(p_t9_val, p_t9_copy_val, &mode, p_tag_map) == 0, "equal fn has failed");

    /* free copy */
    fail_unless(metac_value_free_ex(p_t9_copy_val, &nhmode, NULL, p_tag_map) != 0, "free_ex failed");

    metac_value_delete(p_t9_copy_val);
    metac_value_delete(p_t9_val);
    metac_tag_map_delete(p_tag_map);
}END_TEST

METAC_START_TEST(t10_test) {
    int some_value;
    metac_value_memory_map_mode_t mode = {};
    metac_value_memory_map_non_handled_mode_t nhmode = {};

    metac_tag_map_t *p_tag_map = t9_10_tag_map();
    fail_unless(p_tag_map != NULL, "tagmap is NULL");

    static test10_t t10 = {
        .f = 1.1, .d = 0.1, .ld = 0.2, .fc = -0.1, .dc = -0.2 + I, .ldc = -9.1, 
        .len = 4, .flex = {0, 1, 2, 3},};
    WITH_METAC_DECLLOC(loc, 
        test10_t * p_t10 = &t10;
        test10_t * p_t10_copy = NULL;
    )

    metac_value_t *p_t10_val = METAC_VALUE_FROM_DECLLOC(loc, p_t10);
    metac_value_t *p_t10_copy_val = METAC_VALUE_FROM_DECLLOC(loc, p_t10_copy);
    fail_unless(p_t10_val != NULL && p_t10_copy_val != NULL, "values are NULL");

    fail_unless(metac_value_copy_ex(p_t10_val, p_t10_copy_val, &mode, NULL, NULL, NULL) == NULL , "memory mode copy not failed");
    fail_unless(metac_value_copy_ex(p_t10_val, p_t10_copy_val, &mode, NULL, NULL, p_tag_map)!= NULL, "memory mode copy failed");

    fail_unless(
        p_t10_copy != NULL &&
        p_t10_copy->len == p_t10->len &&
        p_t10_copy->f == p_t10->f &&
        p_t10_copy->d == p_t10->d &&
        p_t10_copy->ld == p_t10->ld &&
        p_t10_copy->fc == p_t10->fc &&
        p_t10_copy->dc == p_t10->dc &&
        p_t10_copy->ldc == p_t10->ldc &&
        p_t10_copy->flex[0] == p_t10->flex[0] &&
        p_t10_copy->flex[1] == p_t10->flex[1] &&
        p_t10_copy->flex[2] == p_t10->flex[2] &&
        p_t10_copy->flex[3] == p_t10->flex[3], "copy failed to copy data");

    fail_unless(metac_value_free_ex(p_t10_copy_val, &nhmode, NULL, p_tag_map) != 0, "free_ex failed");

    metac_value_delete(p_t10_copy_val);
    metac_value_delete(p_t10_val);
    metac_tag_map_delete(p_tag_map);
}END_TEST
