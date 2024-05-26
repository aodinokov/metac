#include "metac/backend/entry.h"

#include <stdlib.h> /*bsearch*/
#include <string.h> /*strdup, strlen*/

static int compare_per_file_item(const void *a, const void *b) {
    struct metac_per_file_item *p_a = (struct metac_per_file_item *)a;
    struct metac_per_file_item *p_b = (struct metac_per_file_item *)b;
    return strcmp(p_a->file, p_b->file);
}

static int compare_per_loc_item(const void *a, const void *b) {
    struct metac_per_loc_item *p_a = (struct metac_per_loc_item *)a;
    struct metac_per_loc_item *p_b = (struct metac_per_loc_item *)b;
    return strcmp(p_a->loc_name, p_b->loc_name);
}

static int compare_per_name_item(const void *a, const void *b) {
    struct metac_per_name_item *p_a = (struct metac_per_name_item *)a;
    struct metac_per_name_item *p_b = (struct metac_per_name_item *)b;
    return strcmp(p_a->name, p_b->name);
}

metac_entry_t * metac_entry_from_entry_db(struct metac_entry_db * p_entry_db, 
    struct metac_decl_loc * p_decl_loc,
    const metac_name_t p_name) {
    if (p_entry_db == NULL ||
        p_decl_loc == NULL ||
        p_decl_loc->name == NULL ||
        p_name == NULL) {
        return NULL;
    }

    struct metac_per_file_item f_key = {.file = p_decl_loc->file,};
    struct metac_per_file_item *p_f = bsearch(&f_key, 
        p_entry_db->per_file_items,
        p_entry_db->per_file_item_count,
        sizeof(struct metac_per_file_item),
        compare_per_file_item);
    if (p_f == NULL)
        return NULL;
    
    struct metac_per_loc_item l_key = {.loc_name = p_decl_loc->name,};
    struct metac_per_loc_item *p_l = bsearch(&l_key, 
        p_f->per_loc_items,
        p_f->per_loc_item_count,
        sizeof(struct metac_per_loc_item),
        compare_per_loc_item);
    if (p_l == NULL)
        return NULL;

    struct metac_per_name_item n_key = {.name = p_name,};
    struct metac_per_name_item *p_n = bsearch(&n_key, 
        p_l->per_name_items,
        p_l->per_name_item_count,
        sizeof(struct metac_per_name_item),
        compare_per_name_item);
    if (p_n == NULL) {
        return NULL;
    }

    if (p_n->entry_count == 1) {
        return p_n->pp_entries[0];
    }
    // can't distinguish - give up
    return NULL;
}
