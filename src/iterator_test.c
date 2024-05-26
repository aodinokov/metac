#include "metac/test.h"
#include "metac/backend/entry.h"

#include "iterator.c"

METAC_START_TEST(test_metac_recursive_iterator_life_use_case) {
    static struct metac_entry int_0000 = {
        .kind = METAC_KND_base_type,    
        .name = "int",    
        .base_type_info = {
            .byte_size = sizeof(int),
            .encoding = METAC_ENC_signed
        },
    };
    static struct metac_entry long_int_0000 = {
        .kind = METAC_KND_base_type,    
        .name = "long int",    
        .base_type_info = {
            .byte_size = sizeof(long int),
            .encoding = METAC_ENC_signed
        },
    };
    struct metac_entry var_data_0000 = {
        .kind = METAC_KND_variable,    
        .name = "data",        
        .variable_info = {        
            .type = &long_int_0000,        
        },
    };
    struct metac_entry var_sum_0000 = {
        .kind = METAC_KND_variable,    
        .name = "sum",        
        .variable_info = {        
            .type = &int_0000,        
        },
    };
    struct {
        metac_entry_t * in;
        metac_name_t expected;
    }tcs[] = {
        {
            .in = &var_data_0000,
            .expected = "long int data",
        },
        {
            .in = &var_sum_0000,
            .expected = "int sum",
        },
    };
    for (int tc_inx = 0; tc_inx < sizeof(tcs)/sizeof(tcs[0]); tc_inx++) {
        metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(tcs[tc_inx].in);
        fail_unless(p_iter != NULL, "tc_inx %d: metac_recursive_iterator_add_current_dep failed", tc_inx);

        for (metac_entry_t * p = (metac_entry_t *)metac_recursive_iterator_next(p_iter);
            p != NULL;
            p = (metac_entry_t *)metac_recursive_iterator_next(p_iter)) {

            int state = metac_recursive_iterator_get_state(p_iter);
            switch(p->kind) {
                case METAC_KND_variable: {
                    if (p->variable_info.type != NULL) {
                        switch (state) {
                            case METAC_R_ITER_start: {
                                metac_recursive_iterator_create_and_append_dep(p_iter, p->variable_info.type);// ask to process type on the next level
                                metac_recursive_iterator_set_state(p_iter, 1);
                                continue;
                            }
                            case 1: {
                                int sz = metac_recursive_iterator_dep_queue_size(p_iter);
                                fail_unless( sz == 1, "expected 1, got %i", sz);
                                fail_unless(metac_recursive_iterator_get_dep_out(p_iter, 1, NULL, NULL) == NULL, "expected only 1 dep");
                                char *_pre_type_name = metac_recursive_iterator_get_dep_out(p_iter, 0, NULL, NULL);
                                char * type_name = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                                fail_unless(_pre_type_name, type_name, "expected pre and dequeue to be equal");
                                if (type_name == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                    continue;
                                }
                                char buf[5];
                                int len = snprintf(buf, sizeof(buf), "%s %s", type_name, p->name);
                                char * res = calloc(len+1, sizeof(*res));
                                if (res == NULL) {
                                    metac_recursive_iterator_fail(p_iter);
                                }
                                snprintf(res, len + 1, "%s %s", type_name, p->name);
                                free(type_name);
                                metac_recursive_iterator_done(p_iter, res);
                                continue;
                            }
                        }
                    }
                }
                break;
                case METAC_KND_base_type: {
                    char * res = strdup(p->name);
                    if (res == NULL) {
                        metac_recursive_iterator_fail(p_iter);
                    }
                    metac_recursive_iterator_done(p_iter, res);
                    continue;
                }
                break;
            }
        }
        char * res = metac_recursive_iterator_get_out(p_iter, NULL, NULL);
        fail_unless((res != NULL) == (tcs[tc_inx].expected != NULL), "tc_inx %d: got %p, expected %p", 
            tc_inx, res, tcs[tc_inx].expected);
        if (tcs[tc_inx].expected != NULL) {
            fail_unless(strcmp(res, tcs[tc_inx].expected) == 0, "tc_inx %d: got %s, expected %s",
            tc_inx, res, tcs[tc_inx].expected);
        }
        free(res);
        metac_recursive_iterator_free(p_iter);
    }
}END_TEST


// START__TEST(test_metac_recursive_iterator_test_failure) {

// }END_TEST