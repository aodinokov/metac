
#include "metac/reflect/entry_tag.h"
#include "metac/reflect/value.h"

#include "metac/backend/hashmap.h"
#include "metac/backend/printf_format.h" /* metac_new_value_vprintf_ex */
#include "metac/backend/value.h" /* for handlers we need metac_value_walker_hierarchy_level */

#include <assert.h> /* assert */
#include <errno.h> /* EINVAL... */
#include <string.h> /* memcpy */

// TODO: use this only where it's not provided (windows)
// source https://opensource.apple.com/source/Libc/Libc-1158.30.7/string/FreeBSD/strndup.c.auto.html
static char * _strndup(const char *str, size_t n) {
	size_t len;
	char *copy;

	for (len = 0; len < n && str[len]; len++) {
		continue;
    }

	if ((copy = malloc(len + 1)) == NULL) {
		return (NULL);
    }
	memcpy(copy, str, len);
	copy[len] = '\0';
	return (copy);
}

/* we're using metac_entry_t* as key */
static int addr_ptr_key_hash_tags(void* key) {
    return hashmapHash(&key, sizeof(void*));
}
static bool addr_ptr_key_equals_tags(void* keyA, void* keyB) {
    return keyA == keyB;
}

struct metac_tag_map {
    metac_num_t categories_count;
    struct metac_tag_map_entry {
        metac_tag_map_entry_category_mask_t mask;
        metac_tag_map_t * p_ext_map;    /* if external map - put here pointer. otherwise NULL */
        Hashmap * p_map;
    }*p_categories;
    metac_entry_tag_t * p_default_tag;  // can be NULL, if not NUll - used for all non-matching
};

static metac_entry_tag_t * _entry_new_tag(metac_entry_tag_t * p_tag) {
    metac_entry_tag_t *p_local_tag = calloc(1, sizeof(*p_local_tag));
    if (p_local_tag == NULL) {
        return NULL;
    }
    memcpy(p_local_tag, p_tag, sizeof(*p_local_tag));
    if (p_tag->p_tagstring != NULL) {
        p_local_tag->p_tagstring = strdup(p_tag->p_tagstring);
        if (p_local_tag->p_tagstring == NULL) {
            free (p_local_tag);
            return NULL;
        }
    }
    return p_local_tag;
}

static void _entry_tag_delete(metac_entry_tag_t *p_tag){
    if (p_tag == NULL) {
        return;
    }

    if (p_tag->p_tagstring != NULL) {
        free(p_tag->p_tagstring);
    }

    if (p_tag->p_context != NULL && p_tag->context_free_handler != NULL) {
        p_tag->context_free_handler(p_tag->p_context);
    }
    free(p_tag);
}

static bool _tag_map_delete_callback(void* key, void* value, void* context) {
    _entry_tag_delete((metac_entry_tag_t *)value);
    return true;
}

void metac_tag_map_delete(metac_tag_map_t * p_tag_map) {
    if (p_tag_map == NULL) {
        return;
    }

    for (metac_num_t i = 0; i< p_tag_map->categories_count; ++i) {
        if (p_tag_map->p_categories[i].p_map != NULL) {
            hashmapForEach(p_tag_map->p_categories[i].p_map, _tag_map_delete_callback, NULL);
            hashmapFree(p_tag_map->p_categories[i].p_map);
        }
    }
    if (p_tag_map->p_categories != NULL) {
        free(p_tag_map->p_categories);
    }

    _entry_tag_delete(p_tag_map->p_default_tag);
    free(p_tag_map);
}

metac_tag_map_t * metac_new_tag_map_ex(metac_num_t categories_count, metac_tag_map_entry_t * p_map_entries, metac_entry_tag_t *p_default_tag) {
    if (categories_count <= 0 || p_map_entries == NULL) {
        return NULL;
    }

    metac_tag_map_t * p_tag_map = calloc(1, sizeof(*p_tag_map));
    if (p_tag_map == NULL) {
        return NULL;
    }

    if (p_default_tag != NULL) {
        p_tag_map->p_default_tag = _entry_new_tag(p_default_tag);
        if (p_tag_map->p_default_tag == NULL) {
            free(p_tag_map);
            return NULL;
        }
    }

    p_tag_map->categories_count = categories_count;
    p_tag_map->p_categories = calloc(categories_count, sizeof(*p_tag_map->p_categories));
    if (p_tag_map->p_categories == NULL) {
        _entry_tag_delete(p_tag_map->p_default_tag);
        free(p_tag_map);
        return NULL;
    }

    for (metac_num_t i = 0; i < categories_count; ++i) {
        p_tag_map->p_categories[i].mask = p_map_entries[i].mask;
        p_tag_map->p_categories[i].p_ext_map = p_map_entries[i].p_ext_map;
        if (p_map_entries[i].p_ext_map == NULL) {
            p_tag_map->p_categories[i].p_map = hashmapCreate(1024, addr_ptr_key_hash_tags, addr_ptr_key_equals_tags);
            if (p_tag_map->p_categories[i].p_map == NULL) {
                metac_tag_map_delete(p_tag_map);
                return NULL;
            }
        }
    }

    return p_tag_map;    
}

metac_tag_map_t * metac_new_tag_map(metac_entry_tag_t *p_default_tag) {
    metac_tag_map_entry_category_mask_t mask = 
        METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) | 
        METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
        METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member);

    return metac_new_tag_map_ex(1, (metac_tag_map_entry_t[]){{.mask = mask, .p_ext_map = NULL}}, p_default_tag);
}

metac_entry_tag_t * metac_tag_map_tag(metac_tag_map_t * p_tag_map, metac_entry_t* p_entry) {
    if (p_tag_map == NULL) {
        return NULL;
    }

    metac_entry_t* p_final_entry = NULL;//metac_entry_final_entry(p_entry, NULL);

    metac_tag_map_entry_category_mask_t mask = 0;
    switch (metac_entry_kind(p_entry)) {
        case METAC_KND_member:
            mask += METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member);
            break;
        case METAC_KND_variable:
            mask += METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable);
            break;
        case METAC_KND_func_parameter:
            mask += METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter);
            break;
    }

    for (metac_num_t i = 0; i< p_tag_map->categories_count; ++i) {
        if ((p_tag_map->p_categories[i].mask & mask) == 0) {
            continue;
        }
        if (p_tag_map->p_categories[i].p_ext_map != NULL) {
            metac_entry_tag_t * p_ext_res = metac_tag_map_tag(p_tag_map->p_categories[i].p_ext_map, p_entry);
            if (p_ext_res != NULL) {
                return p_ext_res;
            }
        }
        if (p_tag_map->p_categories[i].p_map == NULL) {
            assert(0);
            continue;
        }

        metac_entry_tag_t *p_tag = (metac_entry_tag_t *)hashmapGet(p_tag_map->p_categories[i].p_map, (void*)p_entry);
        if (p_tag != NULL) {
            return p_tag;
        }
        if (p_tag_map->p_categories[i].mask & METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_final)){
            if (p_final_entry == NULL) {
                p_final_entry = metac_entry_final_entry(p_entry, NULL);
            }
            if (p_final_entry != NULL) {
                p_tag = (metac_entry_tag_t *)hashmapGet(p_tag_map->p_categories[i].p_map, (void*)p_final_entry);
                if (p_tag != NULL) {
                    return p_tag;
                }
            }
        }
    }

    return p_tag_map->p_default_tag;
}

/* shallow-copies p_tag structure and adds to hashmap of the type
 p_etnry can be variable, member or argument... hmm.. maybe final types also.. because I'm not sure how to deal with variales..... looks like this
 is going to happen for local variables on the spot */
static int _tag_map_set_tag(metac_tag_map_t * p_tag_map, 
    metac_num_t category,
    metac_flag_t override,
    metac_entry_t* p_entry,
    metac_entry_tag_t *p_tag) {

    if (override == 0 && hashmapContainsKey(p_tag_map->p_categories[category].p_map, (void*)p_entry)){
        return -(EALREADY);
    }

    metac_entry_tag_t *p_local_tag = _entry_new_tag(p_tag);
    if (p_local_tag == NULL) {
        return -(ENOMEM);
    }

    metac_entry_tag_t *p_prev_tag = NULL;
    
    if (hashmapPut(p_tag_map->p_categories[category].p_map, (void*)p_entry, (void*)p_local_tag, (void**)&p_prev_tag) != 0) {
        free(p_local_tag);
        return -(EFAULT);
    }

    if (p_prev_tag != NULL) {
        assert(override != 0);
        _entry_tag_delete(p_prev_tag);
    }

    return 0;
}

int metac_tag_map_set_tag(metac_tag_map_t * p_tag_map, 
    metac_num_t category,
    metac_flag_t override,
    metac_entry_t* p_entry,
    metac_tag_map_op_t op,
    metac_entry_tag_t *p_tag) {
    if (p_entry == NULL || p_tag_map == NULL || p_tag == NULL || category < 0 || category >= p_tag_map->categories_count ) {
        return -(EINVAL);
    }

    if (p_tag_map->p_categories[category].p_map == NULL) {
        return -(EINVAL);
    }

    if (op == METAC_TEO_entry) {
        int res = _tag_map_set_tag(p_tag_map, category, override, p_entry, p_tag);
        if (res != 0) {
            return res;
        }
    }

    if (op == METAC_TEO_final_entry) {
        p_entry = metac_entry_final_entry(p_entry, NULL);
        if (p_entry == NULL) {
            return -(EFAULT);
        }
        int res = _tag_map_set_tag(p_tag_map, category, override, p_entry, p_tag);
        if (res != 0) {
            return res;
        }
    }
    return 0;
}

metac_name_t metac_entry_tag_string_lookup(metac_entry_tag_t *p_tag, metac_name_t in_key) {
    if (p_tag == NULL || p_tag->p_tagstring == NULL || in_key == NULL) { 
        return NULL;
    }

    size_t in_key_len = strlen(in_key);
    size_t tag_len = strlen(p_tag->p_tagstring);
    int state = 0;
    size_t key_begin_inx = -1;
    size_t key_len = 0;
    size_t value_q_begin_inx = -1;
    size_t value_q_len = 0;

    for (size_t i = 0; i < tag_len; ++i) {
        switch(state) {
            case -1: /*error state - waiting for next space */
                if(p_tag->p_tagstring[i] != ' ') {
                    continue;
                }
                state = 0;
                continue;
            case 0: /* looking for non-space */
                if(p_tag->p_tagstring[i] == ' ') {
                    continue;
                }
                state = 1;
                key_begin_inx = i;
                continue;
            case 1: /* looking for :, but can fallback if meet space */
                if(p_tag->p_tagstring[i] == ' ') {
                    state = 0;  /* fallback */
                    continue;
                }
                if (p_tag->p_tagstring[i] == ':') {
                    state = 2;
                    key_len = i - key_begin_inx;
                    continue;
                }
                continue;
            case 2: /* next must be ""*/
                if (p_tag->p_tagstring[i] != '"') {
                    state = -1;
                    continue;
                }
                state = 3;
                value_q_begin_inx = i;
                continue;
            case 3: /* looking for the ", but handle \" "*/
                if (p_tag->p_tagstring[i] == '\\') {
                    state = 4;
                    continue;
                }
                if (p_tag->p_tagstring[i] == '"') {
                    value_q_len = i - value_q_begin_inx;

                    if (in_key_len == key_len && 
                        memcmp(&p_tag->p_tagstring[key_begin_inx], in_key, key_len) == 0) {
                        /* get rid of quotes */
                        value_q_begin_inx +=1;
                        value_q_len -= 1;
                        return _strndup(&p_tag->p_tagstring[value_q_begin_inx], value_q_len);
                    }

                    state = 0; /* restart the process*/
                    continue;
                }
                continue;
            case 4: /* accept any characted, because it's after \*/
                state = 3;
                continue;
        }
    }
    return NULL;
}


struct metac_count_by_context * metac_new_count_by_context(metac_name_t counter_sibling_entry_name) {
    struct metac_count_by_context * p_cnxt = calloc(1, sizeof(*p_cnxt));
    if (p_cnxt == NULL) {
        return NULL;
    }
    if (counter_sibling_entry_name != NULL) {
        p_cnxt->counter_sibling_entry_name = strdup(counter_sibling_entry_name);
        if (p_cnxt->counter_sibling_entry_name == NULL) {
            free(p_cnxt);
            return NULL;
        }
    }
    return p_cnxt;
}

void metac_count_by_context_delete(void *p_context) {
    if (p_context == NULL) {
        return;
    }
    struct metac_count_by_context *p_cnxt = (struct metac_count_by_context *)p_context;
    if (p_cnxt->counter_sibling_entry_name != NULL) {
        free(p_cnxt->counter_sibling_entry_name);
    }
    free(p_cnxt);
}

int metac_handle_count_by(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    if (p_ev == NULL) {
        return -(EINVAL);
    }
    if ((p_ev->type == METAC_RQVST_pointer_array_count || p_ev->type == METAC_RQVST_flex_array_count) &&
        metac_value_walker_hierarchy_level(p_hierarchy) > 0) {
        struct metac_count_by_context *p_cnxt = (struct metac_count_by_context *)p_context;
        if (p_cnxt == NULL ||
            p_cnxt->counter_sibling_entry_name == NULL ||
            strcmp(p_cnxt->counter_sibling_entry_name, "") == 0) {
            return -(EINVAL);
        }

        metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
        metac_value_t *p_parent_val = metac_value_walker_hierarchy_value(p_hierarchy, 1);

        if (p_val == NULL || p_parent_val == NULL) {
            return -(EFAULT);
        }

        if (metac_value_has_members(p_parent_val) != 0) {
            for (metac_num_t i = 0; i < metac_value_member_count(p_parent_val); ++i) {
                metac_value_t *p_sibling_val = metac_new_value_by_member_id(p_parent_val, i);
                if (metac_value_name(p_sibling_val) == NULL ||
                    strcmp(metac_value_name(p_sibling_val), p_cnxt->counter_sibling_entry_name) !=0 ) {
                    metac_value_delete(p_sibling_val);
                    continue;
                }
                metac_num_t content_len = 0;
                if (metac_value_num(p_sibling_val, &content_len) != 0) {
                    metac_value_delete(p_sibling_val);
                    return -(EFAULT);
                }
                metac_value_delete(p_sibling_val);
                p_ev->p_return_value =  metac_new_element_count_value(p_val, content_len);
                return 0;
            }
        } else if (metac_value_has_parameter_load(p_parent_val) != 0) {
            for (metac_num_t i = 0; i < metac_value_parameter_count(p_parent_val); ++i) {
                metac_value_t *p_sibling_val = metac_value_parameter_new_item(p_parent_val, i);
                if (metac_value_name(p_sibling_val) == NULL ||
                    strcmp(metac_value_name(p_sibling_val), p_cnxt->counter_sibling_entry_name) !=0 ) {
                    metac_value_delete(p_sibling_val);
                    continue;
                }
                metac_num_t content_len = 0;
                if (metac_value_num(p_sibling_val, &content_len) != 0) {
                    metac_value_delete(p_sibling_val);
                    return -(EFAULT);
                }
                metac_value_delete(p_sibling_val);
                p_ev->p_return_value = metac_new_element_count_value(p_val, content_len);
                return 0;
            }
        } else {
            return -(EFAULT);
        }
    }    
    return 0;
}

int metac_handle_zero_ended_string(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context){
    if (p_ev == NULL) {
        return -(EINVAL);
    }
    if ((p_ev->type == METAC_RQVST_pointer_array_count || p_ev->type == METAC_RQVST_flex_array_count) &&
        metac_value_walker_hierarchy_level(p_hierarchy) >= 0) {
        
        metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
        if (p_val == NULL || metac_value_is_pointer(p_val) == 0) {
            return -(EFAULT);
        }
        metac_entry_t *p_target_entry = metac_entry_final_entry(metac_entry_pointer_entry(metac_value_entry(p_val)), NULL);
        if (p_target_entry == NULL || metac_entry_kind(p_target_entry) != METAC_KND_base_type ||
            metac_entry_name(p_target_entry) == NULL || strcmp(metac_entry_name(p_target_entry), "char") != 0) {
            return -(EFAULT);
        }

        char * ptr = NULL;
        if (metac_value_pointer(p_val, (void**)&ptr) != 0) {
            return -(EFAULT);
        }
        if (ptr == NULL) {
            return -(EFAULT); /* shouldn't be here*/
        }
        metac_num_t content_len = strlen(ptr) + 1;
        p_ev->p_return_value =  metac_new_element_count_value(p_val, content_len);
        return 0;
    }    
    return 0;
}

struct metac_container_of_context * metac_new_container_of_context(metac_size_t member_offset,
    metac_entry_t * p_coontainer_entry) {

    struct metac_container_of_context * p_cnxt = calloc(1, sizeof(*p_cnxt));
    if (p_cnxt == NULL) {
        return NULL;
    }

    p_cnxt->member_offset = member_offset;
    p_cnxt->p_coontainer_entry = p_coontainer_entry;

    return p_cnxt;
}
void metac_container_of_context_delete(void *p_context) {
    if (p_context == NULL) {
        return;
    }
    struct metac_container_of_context *p_cnxt = (struct metac_container_of_context *)p_context;
    free(p_cnxt);
}
int metac_handle_container_of(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    if (p_ev == NULL) {
        return -(EINVAL);
    }
    if (p_ev->type == METAC_RQVST_pointer_array_count) {
        struct metac_container_of_context *p_cnxt = (struct metac_container_of_context *)p_context;
        if (p_cnxt == NULL || p_cnxt->p_coontainer_entry == NULL) {
            return -(EINVAL);
        }
        metac_value_t *p_ptr_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
        void * p = NULL;
        if (metac_value_pointer(p_ptr_val, &p) == 0) {
            p = p - p_cnxt->member_offset;
            metac_value_t * p_container_ptr_val =  metac_new_value(p_cnxt->p_coontainer_entry, &p);
            if (p_container_ptr_val == NULL) {
                return -(ENOMEM);
            }
            p_ev->p_return_value = metac_new_element_count_value(p_container_ptr_val, 1);
            metac_value_delete(p_container_ptr_val);
            return 0;
        }
    }
    return 0;
}

struct metac_union_member_select_by_context * metac_new_union_member_select_by_context(metac_name_t sibling_fieldname,
    struct metac_union_member_select_by_case * p_cases, size_t case_sz) {
    struct metac_union_member_select_by_context * p_cnxt = calloc(1, sizeof(*p_cnxt));
    if (p_cnxt == NULL) {
        return NULL;
    }
    if (sibling_fieldname != NULL) {
        p_cnxt->sibling_selector_fieldname = strdup(sibling_fieldname);
        if (p_cnxt->sibling_selector_fieldname == NULL) {
            free(p_cnxt);
            return NULL;
        }
    }
    if (case_sz > 0) {
        assert((case_sz % sizeof(struct metac_union_member_select_by_case)) == 0);
        p_cnxt->cases_count = case_sz / sizeof(struct metac_union_member_select_by_case);
        p_cnxt->p_cases = calloc(p_cnxt->cases_count, sizeof(struct metac_union_member_select_by_context));
        if (p_cnxt->p_cases == NULL) {
            if (p_cnxt->sibling_selector_fieldname != NULL) {
                free(p_cnxt->sibling_selector_fieldname);
            }
            free(p_cnxt);
            return NULL;  
        }
        memcpy(p_cnxt->p_cases, p_cases, case_sz);
    }
    return p_cnxt;
}

void metac_union_member_select_by_context_delete(void *p_context) {
    if (p_context == NULL) {
        return;
    }
    struct metac_union_member_select_by_context *p_cnxt = (struct metac_union_member_select_by_context *)p_context;
    if (p_cnxt->sibling_selector_fieldname != NULL) {
        free(p_cnxt->sibling_selector_fieldname);
    }
    if (p_cnxt->p_cases != NULL) {
        free(p_cnxt->p_cases);
    }
    free(p_cnxt);

}

int metac_handle_union_member_select_by(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    if (p_ev == NULL) {
        return -(EINVAL);
    }
    if (p_ev->type == METAC_RQVST_union_member && 
        metac_value_walker_hierarchy_level(p_hierarchy) > 0) {

        struct metac_union_member_select_by_context *p_cnxt = (struct metac_union_member_select_by_context *)p_context;
        if (p_cnxt == NULL ||
            p_cnxt->sibling_selector_fieldname == NULL ||
            strcmp(p_cnxt->sibling_selector_fieldname, "") == 0) {
            return -(EINVAL);
        }

        metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
        metac_value_t *p_parent_val = metac_value_walker_hierarchy_value(p_hierarchy, 1);
        if (p_val == NULL || p_parent_val == NULL || 
            metac_value_has_members(p_val) == 0 ||
            metac_value_has_members(p_parent_val) == 0) {
            return -(EINVAL);
        }

        for (metac_num_t i = 0; i < metac_value_member_count(p_parent_val); ++i) {
            metac_value_t *p_sibling_val = metac_new_value_by_member_id(p_parent_val, i);
            if (metac_value_name(p_sibling_val) == NULL ||
                strcmp(metac_value_name(p_sibling_val),p_cnxt->sibling_selector_fieldname) !=0 ) {
                metac_value_delete(p_sibling_val);
                continue;
            }

            metac_num_t id;
            if (metac_value_num(p_sibling_val, &id) != 0) {
                metac_value_delete(p_sibling_val);
                return -(EFAULT);
            }   
            metac_value_delete(p_sibling_val);
            /* val contains the value read from sibling field */
            if (p_cnxt->cases_count > 0) {
                metac_flag_t id_converted = 0;
                for (metac_num_t i = 0; i < p_cnxt->cases_count; ++i) {
                    if (p_cnxt->p_cases[i].fld_val == id) {
                        if (p_cnxt->p_cases[i].union_fld_name != NULL) {
                            id = metac_entry_member_name_to_id(metac_value_entry(p_val), p_cnxt->p_cases[i].union_fld_name);
                            if (id < 0) {
                                return -(EFAULT);
                            }
                        } else {
                            id = p_cnxt->p_cases[i].union_fld_id;
                        }
                        id_converted = 1;
                        break;
                    }
                }
                if (id_converted == 0) {
                    return -(ENOENT);
                }
            }
            p_ev->p_return_value = metac_new_value_by_member_id(p_val, id);
            return 0;
        }
    }
    return 0;
}

struct metac_ptr_cast_context * metac_new_ptr_cast_context(
    metac_name_t sibling_count_fieldname,
    metac_name_t sibling_selector_fieldname,
    struct metac_ptr_cast_case * p_cases, size_t case_sz) {

    if (sibling_selector_fieldname == NULL) {
        return NULL;
    }

    struct metac_ptr_cast_context * p_cnxt = calloc(1, sizeof(*p_cnxt));
    if (p_cnxt == NULL) {
        return NULL;
    }

    if (sibling_count_fieldname != NULL) {
        p_cnxt->sibling_count_fieldname = strdup(sibling_count_fieldname);
        if (p_cnxt->sibling_count_fieldname == NULL) {
            free(p_cnxt);
            return NULL;
        }
    }

    if (sibling_selector_fieldname != NULL) {
        p_cnxt->sibling_selector_fieldname = strdup(sibling_selector_fieldname);
        if (p_cnxt->sibling_selector_fieldname == NULL) {
            if (p_cnxt->sibling_count_fieldname != NULL) {
                free(p_cnxt->sibling_count_fieldname);
            }
            free(p_cnxt);
            return NULL;
        }
    }
    if (case_sz > 0) {
        assert((case_sz % sizeof(struct metac_ptr_cast_case)) == 0);
        p_cnxt->cases_count = case_sz / sizeof(struct metac_ptr_cast_case);
        p_cnxt->p_cases = calloc(p_cnxt->cases_count, sizeof(struct metac_ptr_cast_case));
        if (p_cnxt->p_cases == NULL) {
            if (p_cnxt->sibling_selector_fieldname != NULL) {
                free(p_cnxt->sibling_selector_fieldname);
            }
            if (p_cnxt->sibling_count_fieldname != NULL) {
                free(p_cnxt->sibling_count_fieldname);
            }
            free(p_cnxt);
            return NULL;  
        }
        memcpy(p_cnxt->p_cases, p_cases, case_sz);
    }
    return p_cnxt;
}

void metac_ptr_cast_context_delete(void *p_context) {
    if (p_context == NULL) {
        return;
    }
    struct metac_ptr_cast_context *p_cnxt = (struct metac_ptr_cast_context *)p_context;

    if (p_cnxt->sibling_selector_fieldname != NULL) {
        free(p_cnxt->sibling_selector_fieldname);
    }
    if (p_cnxt->sibling_count_fieldname != NULL) {
        free(p_cnxt->sibling_count_fieldname);
    }
    if (p_cnxt->p_cases != NULL) {
        free(p_cnxt->p_cases);
    }
    free(p_cnxt);
}

int metac_handle_ptr_cast(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    if (p_ev == NULL) {
        return -(EINVAL);
    }
    if (p_ev->type == METAC_RQVST_pointer_array_count && 
        metac_value_walker_hierarchy_level(p_hierarchy) > 0) {

        struct metac_ptr_cast_context *p_cnxt = (struct metac_ptr_cast_context *)p_context;
        if (p_cnxt == NULL ||
            p_cnxt->sibling_selector_fieldname == NULL ||
            strcmp(p_cnxt->sibling_selector_fieldname, "") == 0 ||
            p_cnxt->cases_count <= 0) {
            return -(EINVAL);
        }

        metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
        metac_value_t *p_parent_val = metac_value_walker_hierarchy_value(p_hierarchy, 1);
        if (p_val == NULL || p_parent_val == NULL || metac_value_has_members(p_parent_val) == 0) {
            return -(EINVAL);
        }

        metac_num_t content_type = 0;
        metac_num_t type_id = metac_entry_member_name_to_id(metac_value_entry(p_parent_val), p_cnxt->sibling_selector_fieldname);
        if (type_id < 0) {
            return -(EINVAL);
        }
        metac_value_t *p_sibling_val = metac_new_value_by_member_id(p_parent_val, type_id);
        if (metac_value_num(p_sibling_val, &content_type) != 0) {
            metac_value_delete(p_sibling_val);
            return -(EINVAL);
        } 
        metac_value_delete(p_sibling_val);

        metac_num_t content_count = 1;
        if (p_cnxt->sibling_count_fieldname != NULL && strcmp(p_cnxt->sibling_count_fieldname, "") != 0) {
            metac_num_t count_id = metac_entry_member_name_to_id(metac_value_entry(p_parent_val), p_cnxt->sibling_count_fieldname);

            metac_value_t *p_sibling_val = metac_new_value_by_member_id(p_parent_val, count_id);
            if (metac_value_num(p_sibling_val, &content_count) != 0) {
                metac_value_delete(p_sibling_val);
                return -(EINVAL);
            } 
            metac_value_delete(p_sibling_val);
        }

        for (metac_num_t i  = 0; i < p_cnxt->cases_count; ++i) {
            if (content_type == p_cnxt->p_cases[i].fld_val) {
                if (p_cnxt->p_cases[i].p_ptr_entry == NULL) {
                    return -(EINVAL);
                }
                metac_value_t *p_cast_ptr_val = metac_new_value(p_cnxt->p_cases[i].p_ptr_entry, metac_value_addr(p_val));
                if (p_cast_ptr_val == NULL) {
                    return -(ENOMEM);
                }
                p_ev->p_return_value = metac_new_element_count_value(p_cast_ptr_val, content_count);
                metac_value_delete(p_cast_ptr_val);
                return 0;
            }
        }
    }
    return 0;
}

int metac_handle_printf_format(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context) {
    if (p_ev == NULL) {
        return -(EINVAL);
    }
    if (p_ev->type != METAC_RQVST_va_list ||
        p_ev->p_va_list_container == NULL ||
        metac_value_walker_hierarchy_level(p_hierarchy) < 0) {
        return -(EINVAL);
    }
    metac_value_t *p_val = metac_value_walker_hierarchy_value(p_hierarchy, 0);
    metac_entry_t *p_va_list_entry = metac_entry_by_paremeter_id(metac_value_entry(p_val), p_ev->va_list_param_id);
    metac_value_t *p_param_val = metac_value_parameter_new_item(p_val, p_ev->va_list_param_id -1 /* use previous param */);

    if (p_va_list_entry == NULL) {
        return -(EINVAL);
    }

    if (p_param_val == NULL) {
        return -(EINVAL);
    }

    if (metac_value_is_pointer(p_param_val) == 0) {
        metac_value_delete(p_param_val);
        return -(EINVAL);
    }

    // extract pointer
    char * format = NULL;
    if (metac_value_pointer(p_param_val, (void **)&format) != 0) {
        metac_value_delete(p_param_val);
        return -(EINVAL);
    }
    metac_value_delete(p_param_val);
    /* potentially we could check if that is char *, but this is optional*/

    if (format == NULL) {
        return -(EINVAL);
    }

    assert(0);
    //p_ev->p_return_value = metac_new_value_vprintf_ex(format, p_va_list_entry, p_ev->p_va_list_container->parameters);
    return 0;
}
