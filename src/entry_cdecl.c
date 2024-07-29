#include "metac/backend/string.h"
#include "metac/backend/entry.h"
#include "metac/backend/iterator.h"

#ifndef dsprintf
dsprintf_render_with_buf(64)
#define dsprintf(_x_...) dsprintf64( _x_)
#endif

#include <assert.h>

/**
 * convert metac_entry to the C-declaration like string.
 * if entry is type the result will container a placeholder %s
 * which can be used to put variable to or change to empty string.
*/
char * metac_entry_cdecl(metac_entry_t * p_entry) {
    if (p_entry == NULL) {
        return NULL;
    }
    metac_recursive_iterator_t * p_iter = metac_new_recursive_iterator(p_entry);

    for (metac_entry_t * p = (metac_entry_t *)metac_recursive_iterator_next(p_iter); p != NULL;
        p = (metac_entry_t *)metac_recursive_iterator_next(p_iter)) {
        int state = metac_recursive_iterator_get_state(p_iter);
        switch(p->kind) {
            case METAC_KND_variable: {
                switch (state) {
                    case METAC_R_ITER_start: {
                        if (p->variable_info.type != NULL) {
                            metac_recursive_iterator_create_and_append_dep(p_iter, p->variable_info.type);// ask to process type on the next level
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                         continue;
                    }
                    case 1: {
                        char * type_pattern = NULL;
                        if (p->variable_info.type != NULL) {
                            type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                            if (type_pattern == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                        }
                        char * res = dsprintf((type_pattern != NULL)?type_pattern:"void %s",
                            p->name);
                        if (type_pattern != NULL) {
                            free(type_pattern);
                        }
                        if (res == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        metac_recursive_iterator_done(p_iter, res);
                        continue;
                    }
                }
            }
            break;
            case METAC_KND_typedef: /* the same implementation - typedef and base type must have name */
            case METAC_KND_base_type: {
                char * res = dsprintf("%s %%s",p->name);
                if (res == NULL) {
                    metac_recursive_iterator_fail(p_iter);
                }
                metac_recursive_iterator_done(p_iter, res);
                continue;
            }
            break;
            case METAC_KND_enumeration_type: {
                if (p->name == NULL) {
                    char * val = NULL;
                    for (int i = 0; i < p->enumeration_type_info.enumerators_count; ++i) {
                        char * new_val = dsprintf("%s%s%s = %ld,",
                                (val == NULL)?"":val,
                                (val == NULL)?"":" ",
                                p->enumeration_type_info.enumerators[i].name,
                                p->enumeration_type_info.enumerators[i].const_value);
                        if (val != NULL) {
                            free(val);
                        }
                        val = new_val;
                        if (val == NULL) {
                            break; /* will be handled right after the cycle */
                        }
                    }
                    if (p->enumeration_type_info.enumerators_count > 0 && val == NULL) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                    char * new_val = dsprintf("enum {%s} %%s", (val == NULL)?"":val);
                    if (val != NULL) {
                        free(val);
                    }
                    val = new_val;
                    if (val == NULL) {
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                    metac_recursive_iterator_done(p_iter, val);
                    continue;
                }
                char * val = dsprintf("enum %s %%s", p->name);
                if (val == NULL) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
                metac_recursive_iterator_done(p_iter, val);
                continue;
            }
            break;
            case METAC_KND_array_type: {
                switch (state) {
                    case METAC_R_ITER_start: {
                        if (p->array_type_info.type != NULL) {
                            metac_recursive_iterator_create_and_append_dep(p_iter, p->array_type_info.type);
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                    case 1: {
                        char * type_pattern = NULL;
                        if (p->array_type_info.type != NULL) {
                            type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                            if (type_pattern == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                        }
                        char size_str[64] = "%s[]";
                        if (p->array_type_info.count >= 0){
                            snprintf(size_str, sizeof(size_str),"%%s[%" METAC_COUNT_PRI "]", p->array_type_info.count);
                        }
                        /*printf("returned %s, size_str %s\n", (type_pattern != NULL)?type_pattern:"void %%s", size_str);*/
                        char * res = dsprintf((type_pattern != NULL)?type_pattern:"void %s",
                            size_str);
                        if (type_pattern != NULL) {
                            free(type_pattern);
                        }
                        if (res == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        metac_recursive_iterator_done(p_iter, res);
                        continue;
                    }
                }
            }
            break;
            case METAC_KND_const_type: /* the same implementation for all */
            case METAC_KND_volatile_type:
            case METAC_KND_restrict_type:
            case METAC_KND_mutable_type:
            case METAC_KND_shared_type:
            case METAC_KND_packed_type: {
                switch (state) {
                    case METAC_R_ITER_start: {
                        if (p->qual_type_info.type != NULL) {
                            metac_recursive_iterator_create_and_append_dep(p_iter, p->qual_type_info.type);
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                    case 1: {
                        char * type_pattern = NULL;
                        if (p->qual_type_info.type != NULL) {
                            type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                            if (type_pattern == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                        }
                        char * qual_str = NULL;
                        switch (p->kind) {
                            case METAC_KND_const_type: qual_str = "const %s"; break;
                            case METAC_KND_volatile_type: qual_str = "volatile %s"; break;
                            case METAC_KND_restrict_type: qual_str = "restrict %s"; break;
                            case METAC_KND_mutable_type: qual_str = "mutable %s"; break;
                            case METAC_KND_shared_type: qual_str = "shared %s"; break;
                            case METAC_KND_packed_type: qual_str = "packed %s"; break;
                        }
                        if (qual_str == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        char * res = NULL;
                        if (p->qual_type_info.type != NULL && p->qual_type_info.type->kind == METAC_KND_pointer_type) {
                            /* this is more universal way... maybe we will switch to that*/
                            /*printf("PTR: returned %s, qual_str %s\n", (type_pattern != NULL)?type_pattern:"void %%s", qual_str);*/
                            res = dsprintf((type_pattern != NULL)?type_pattern:"void %s",
                                qual_str);
                        } else {
                            /*printf("NONPTR: returned %s, qual_str %s\n", (type_pattern != NULL)?type_pattern:"void %%s", qual_str);*/
                            res = dsprintf(qual_str,
                                (type_pattern != NULL)?type_pattern:"void %s");
                        }
                        if (type_pattern != NULL) {
                            free(type_pattern);
                        }
                        if (res == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        metac_recursive_iterator_done(p_iter, res);
                        continue;
                    }
                }
            }
            break;
            case METAC_KND_pointer_type: {
                switch (state) {
                     case METAC_R_ITER_start: {
                        if (p->pointer_type_info.type != NULL) {
                            metac_recursive_iterator_create_and_append_dep(p_iter, p->pointer_type_info.type);
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                    case 1: {
                        char * type_pattern = NULL;
                        if (p->pointer_type_info.type != NULL) {
                            type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                            if (type_pattern == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                        }
                        char * res = dsprintf((type_pattern != NULL)?type_pattern:"void %s",
                            "* %s");
                        if (type_pattern != NULL) {
                            free(type_pattern);
                        }
                        if (res == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        metac_recursive_iterator_done(p_iter, res);
                        continue;
                    }
                }
            }
            break;
            case METAC_KND_struct_type:
            case METAC_KND_union_type: {
                char * kind_str = NULL;
                switch (p->kind) {
                    case METAC_KND_struct_type: kind_str = "struct"; break;
                    case METAC_KND_union_type: kind_str = "union"; break;
                }
                if (kind_str == NULL) {
                    metac_recursive_iterator_fail(p_iter);
                    continue;
                }
                switch (state) {
                    case METAC_R_ITER_start: {
                        if (p->name != NULL) { /* simple case -we can reference to the struct/union name*/
                            char * res = dsprintf("%s %s %%s",kind_str, p->name);
                            if (res == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                            metac_recursive_iterator_done(p_iter, res);
                            continue;
                        }
                        // otherwise we have anonimous data type and we'll need to identify it by declaration
                        for (int i = 0; i < p->structure_type_info.members_count; ++i) {
                            if (p->structure_type_info.members[i].member_info.type != NULL) {
                                metac_recursive_iterator_create_and_append_dep(p_iter, p->structure_type_info.members[i].member_info.type);
                            }
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                    case 1: {
                        metac_flag_t failure = 0;
                        char * res = NULL;
                        // we can get here only if the type is anonimous 
                        for (int i = 0; i < p->structure_type_info.members_count; ++i) {
                            char * type_pattern = NULL;
                            if (p->structure_type_info.members[i].member_info.type != NULL) {
                                type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                                if (type_pattern == NULL) {
                                    failure = 1;
                                    break;
                                }
                            }
                            // build name with bits if it's provided
                            char *name = NULL;
                            if (p->structure_type_info.members[i].name != NULL) {
                                if (p->structure_type_info.members[i].member_info.p_bit_size != NULL) {
                                    name = dsprintf("%s:%" METAC_SIZE_PRI,
                                        p->structure_type_info.members[i].name,
                                        *p->structure_type_info.members[i].member_info.p_bit_size);
                                } else {
                                    name = dsprintf("%s",
                                        p->structure_type_info.members[i].name);        
                                }
                            }
                            // build field out of returned pattern and field name (can be absent)
                            char *field = trim_trailing_spaces(dsprintf((
                                type_pattern != NULL)?type_pattern:"void %s",
                                (name != NULL)?name:""));
                            if (type_pattern != NULL) {
                                free(type_pattern);
                            }
                            if (name != NULL) {
                                free(name);
                            }
                            if (field == NULL) {
                                failure = 1;
                                break;
                            }
                            // add field to the string with all fields
                            char *new_res = dsprintf("%s%s; ",
                                (res != NULL)?res:"",
                                field);
                            free(field);
                            if (res != NULL) {
                                free(res);
                            }
                            if (new_res == NULL) {
                                failure = 1;
                                break;
                            }
                            res = new_res;
                        }
                        if (failure != 0) {
                            if (res != NULL) {
                                free(res);
                            }
                            metac_recursive_iterator_set_state(p_iter, 2);
                            continue;
                        }
                        // res contains all fields. wrap it
                        char *new_res = dsprintf("%s {%s} %%s",
                           kind_str,
                            (res != NULL)?res:"");
                        if (res != NULL) {
                            free(res);
                        }
                        if (new_res == NULL) {
                            metac_recursive_iterator_set_state(p_iter, 2);
                            continue;
                        }
                        res = new_res;
                        metac_recursive_iterator_done(p_iter, res);
                        continue;
                    }
                    case 2: { // fail-cleanup
                        while(!metac_recursive_iterator_dep_queue_is_empty(p_iter)) {
                            char * type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                            if (type_pattern != NULL){
                                free(type_pattern);
                            }
                        }
                        metac_recursive_iterator_fail(p_iter);
                        continue;
                    }
                }
            }
            break;
            case METAC_KND_subprogram: {
                switch (state) {
                    case METAC_R_ITER_start: {
                        if (p->subprogram_info.type != NULL) {
                            metac_recursive_iterator_create_and_append_dep(p_iter, p->subprogram_info.type);// ask to process type on the next level
                        }
                        for (int i = 0; i < p->subprogram_info.parameters_count; ++i) {
                            if (p->subprogram_info.parameters[i].kind == METAC_KND_func_parameter) {
                                if (p->subprogram_info.parameters[i].subprogram_parameter_info.unspecified_parameters == 0 &&
                                    p->subprogram_info.parameters[i].subprogram_parameter_info.type != NULL) {
                                    metac_recursive_iterator_create_and_append_dep(p_iter, p->subprogram_info.parameters[i].subprogram_parameter_info.type);
                                }
                            }
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                        continue;
                    }
                    case 1: {
                        metac_flag_t failure = 0;
                        char * type_pattern = NULL;
                        if (p->subprogram_info.type != NULL) {
                            type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                            if (type_pattern == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                        }
                        char * params = NULL;
                        for (int i = 0; i < p->subprogram_info.parameters_count; ++i) {
                            if (p->subprogram_info.parameters[i].kind == METAC_KND_func_parameter) {
                                char *param = NULL;
                                if (p->subprogram_info.parameters[i].subprogram_parameter_info.unspecified_parameters == 0 &&
                                    p->subprogram_info.parameters[i].subprogram_parameter_info.type != NULL) {
                                    char * param_type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                                    if (param_type_pattern == NULL) {
                                        failure = 1;
                                        break;
                                    }
                                    // add param name or space if we don't have name
                                    param = trim_trailing_spaces(dsprintf(param_type_pattern,
                                        (p->subprogram_info.parameters[i].name != NULL)?p->subprogram_info.parameters[i].name:""));
                                    free(param_type_pattern);
                                }else if (p->subprogram_info.parameters[i].subprogram_parameter_info.unspecified_parameters != 0) {
                                    param = strdup("...");
                                }
                                // param has to be non NULL
                                if (param == NULL) {
                                    failure = 1;
                                    break;
                                }
                                // concat to previous param
                                char * new_params = dsprintf("%s%s%s",
                                    (params != NULL)?params:"",
                                    (params != NULL)?", ":"",
                                    param);
                                if (params != NULL) {
                                    free(params);
                                }
                                if (param != NULL) {
                                    free(param);
                                }
                                if (new_params == NULL) {
                                    failure = 1;
                                    break;
                                }
                                params = new_params;
                            }
                        }
                        if (failure != 0){
                            if (params != NULL) {
                                free(params);
                            }                                    
                            if (type_pattern != NULL) {
                                free(type_pattern);
                            }
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        // append type_pattern with (%s)
                        char * subprogram_pattern = dsprintf("%s(%s)",
                            (type_pattern != NULL)?type_pattern:"void %s",
                            "%s");
                        if (type_pattern != NULL) {
                            free(type_pattern);
                        }
                        if (subprogram_pattern == NULL) {
                            if (params != NULL) {
                                free(params);
                            }
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        // put everything into pattern
                        char * res = dsprintf(subprogram_pattern,
                            p->name,
                            (params!=NULL)?params:"");
                        if (subprogram_pattern != NULL) {
                            free(subprogram_pattern);
                        }
                        if (params != NULL) {
                            free(params);
                        }
                        if (res == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        metac_recursive_iterator_done(p_iter, res);
                        continue;
                    }
                }
            }
            break;
            case METAC_KND_subroutine_type:{
                switch (state) {
                    case METAC_R_ITER_start: {
                        if (p->subroutine_type_info.type != NULL) {
                            metac_recursive_iterator_create_and_append_dep(p_iter, p->subroutine_type_info.type);// ask to process type on the next level
                        }
                        for (int i = 0; i < p->subroutine_type_info.parameters_count; ++i) {
                            if (p->subprogram_info.parameters[i].kind == METAC_KND_func_parameter) {
                                if (p->subroutine_type_info.parameters[i].subprogram_parameter_info.unspecified_parameters == 0 &&
                                    p->subroutine_type_info.parameters[i].subprogram_parameter_info.type != NULL) {
                                    metac_recursive_iterator_create_and_append_dep(p_iter, p->subroutine_type_info.parameters[i].subprogram_parameter_info.type);
                                }
                            }
                        }
                        metac_recursive_iterator_set_state(p_iter, 1);
                         continue;
                    }
                    case 1: {
                        metac_flag_t failure = 0;
                        char * type_pattern = NULL;
                        if (p->subroutine_type_info.type != NULL) {
                            type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                            if (type_pattern == NULL) {
                                metac_recursive_iterator_fail(p_iter);
                                continue;
                            }
                        }
                        char * params = NULL;
                        for (int i = 0; i < p->subroutine_type_info.parameters_count; ++i) {
                            assert(p->subprogram_info.parameters[i].kind == METAC_KND_func_parameter);
                            char *param = NULL;
                            if (p->subroutine_type_info.parameters[i].subprogram_parameter_info.unspecified_parameters == 0 &&
                                p->subroutine_type_info.parameters[i].subprogram_parameter_info.type != NULL) {
                                char * param_type_pattern = metac_recursive_iterator_dequeue_and_delete_dep(p_iter, NULL, NULL);
                                if (param_type_pattern == NULL) {
                                    failure = 1;
                                    break;
                                }
                                // we don't have param name, put space there
                                param = trim_trailing_spaces(dsprintf(param_type_pattern,""));
                                free(param_type_pattern);
                            }else if (p->subroutine_type_info.parameters[i].subprogram_parameter_info.unspecified_parameters != 0) {
                                param = strdup("...");
                            }
                            // param has to be non NULL
                            if (param == NULL) {
                                failure = 1;
                                break;
                            }
                            // concat to previous param
                            char * new_params = dsprintf("%s%s%s",
                                (params != NULL)?params:"",
                                (params != NULL)?", ":"",
                                param);
                            if (params != NULL) {
                                free(params);
                            }
                            if (param != NULL) {
                                free(param);
                            }
                            if (new_params == NULL) {
                                failure = 1;
                                break;
                            }
                            params = new_params;
                        }
                        if (failure != 0) {
                            if (params != NULL) {
                                free(params);
                            }                                    
                            if (type_pattern != NULL) {
                                free(type_pattern);
                            }
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        // append type_pattern with (%s)
                        char * subroutine_pattern = dsprintf("%s(%s)",
                            (type_pattern != NULL)?type_pattern:"void %s",
                            "%s");
                        if (type_pattern != NULL) {
                            free(type_pattern);
                        }
                        if (subroutine_pattern == NULL) {
                            if (params != NULL) {
                                free(params);
                            }
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        // now we can form final result
                        char * res = dsprintf(subroutine_pattern,
                            "(%s)",
                            (params!=NULL)?params:"");
                        free(subroutine_pattern);
                        if (params != NULL) {
                            free(params);
                        }
                        if (res == NULL) {
                            metac_recursive_iterator_fail(p_iter);
                            continue;
                        }
                        metac_recursive_iterator_done(p_iter, res);
                        continue;
                    }
                }
            }break;
            default: {
                /*quickly fail if we don't know how to handle*/
                metac_recursive_iterator_fail(p_iter);
                continue;
            }
        }
    }
    char * res = metac_recursive_iterator_get_out(p_iter, NULL, NULL);
    metac_recursive_iterator_free(p_iter);
    return res;
}
