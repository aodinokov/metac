#include "metac/test.h"

#include "entry.c"
#include "entry_db.c"

METAC_START_TEST(test_metac_entry_by_member_ids) {
    WITH_METAC_DECLLOC(loc, 
    struct{
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
    }val);
    metac_entry_t * p_res_entry;
    metac_entry_t *p_entry = metac_entry_final_entry(METAC_ENTRY_FROM_DECLLOC(loc, val), NULL);
    fail_unless(p_entry != NULL, "expected non-null");

    p_res_entry = METAC_ENTRY_BY_MEMBER_IDS(p_entry, 1, {.n="selector"});
    fail_unless(p_res_entry!= NULL, "expected non null");
    fail_unless(p_res_entry->kind == METAC_KND_base_type && 
        strcmp(p_res_entry->name, "int")==0, "expected int, got %s", p_res_entry->name);

    p_res_entry = METAC_ENTRY_BY_MEMBER_IDS(p_entry, 1, {.i=1},{.n="c"});
    fail_unless(p_res_entry!= NULL, "expected non null");
    fail_unless(p_res_entry->kind == METAC_KND_base_type && 
        strcmp(p_res_entry->name, "char")==0, "expected char, got %s", p_res_entry->name);

    p_res_entry = METAC_ENTRY_BY_MEMBER_IDS(p_entry, 1, {.i=4},{.i=2},{.n="b2"});
    fail_unless(p_res_entry!= NULL, "expected non null");
    fail_unless(p_res_entry->kind == METAC_KND_base_type && 
        strcmp(p_res_entry->name, "char")==0, "expected char, got %s", p_res_entry->name);

    p_res_entry = METAC_ENTRY_BY_MEMBER_IDS(p_entry, 1, {.n="sgnd"},{.n="ll"});
    fail_unless(p_res_entry!= NULL, "expected non null");
    fail_unless(p_res_entry->kind == METAC_KND_base_type && 
        strcmp(p_res_entry->name, "long long int")==0, "expected long long int, got %s", p_res_entry->name);

    p_res_entry = METAC_ENTRY_BY_MEMBER_IDS(p_entry, 1, {.n="w"},{.n="d"});
    fail_unless(p_res_entry!= NULL, "expected non null");
    fail_unless(p_res_entry->kind == METAC_KND_base_type && 
        strcmp(p_res_entry->name, "long int")==0, "expected long int, got %s", p_res_entry->name);

}END_TEST