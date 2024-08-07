#ifndef INCLUDE_METAC_ENTRY_TAG_H_
#define INCLUDE_METAC_ENTRY_TAG_H_

#include "metac/reflect/entry.h"

/** \page work_with_tagmap Work with tagmap

tagmap - separate stucture that contains tags bind to metac_entries
tags are the helpers for different modules used for adjustment of the module behavior
here we like in go are using text p_tagstrings whilch can be set

metac_value also needs some hint on how how behave and we're
adding metac_value specific part which isn't text based for performance purpose
    void *p_context;
    metac_value_event_handler_t handler;
    void (*context_free_handler)(void *p_context);

metac_value specific tags help to react to the 3 situations which C-syntax
is ambigious about:
1. which path to select in union. it's important for deep operations like copy, delete, because some of the union paths can have pointers
2. what is a flexible array length. This is improtant for deep operations copy and equal.
3. what is the length of array which the pointer points. by default it's 1, but there can be many different scenarios. This handler also allows
to handle 'containerof' scenario and cast scenario.

Without metac_value part if we had only string tags this module could be completely independent.
But since we support this entry_tag represents the leason on how metac_value can make decisions using metadata + tags.

all deep functions accept metac_tag_map_t as one of arguments which is a db to find additional information
by entry.

the metac_tag_mat_t is flexible and allows to layer tag information from more speicif to less specific if needed.
e.g. we can create.
the mos specific is info attached to the entries like Variable, member and argument.
this will never overlap.

it's also would be nice to try to be able to attach information to the 'final' types to which vars, members or arguments can point.
if nothing found tag map also allows to return some default tag.

tagmap must support the way to reuse other memmaps parts. for that purpose we're allowing to
'stack' them. we're actually creating functions that add data to tagmap provided as argument.
There is a bit tricky approach with variables especially local, since they're only available in the local scope,
we have a bit different macro to have extra argument and pass etry of the variable itself.

there are 4 types of input entires:
variable, member, argument and finaltype.

we may want to have a 'module level' tagmap which is caclulated once.
and we may want to create another tagmap which is for local variables only and
which will fallback to the module level in case it wasn't able to find anyting in local.
or maybe the order should be opposite, it's hard to tell now. so we probablly want to support all cases.

another scenario is - we want to be able to set to say that char * as that particula memmber is different handler (e.g. with len as sibling field),
but in general the final type char * must be zero-ended string.

that gives us the order of handling:
variables->args and memeber->final types.
but the question is still opened if the hander of the same final type must be indepeded on the original kind.
e.g. should char* from variable be handled differently then char* from member? it's unclear.
But we probably can make this configurable

```
enum category {
    VAR = 0,
    ARG,
    MEMBER,
    FINAL,
}

struct level {
 uint8_t mask; //(1<<category)
 metac_tag_map_t * p_external; // is used if is't NULL. othervise we'll create hash. set won't work for this level
}

metac_tag_map_t * metac_new_tag_map_ex(int levels, );
```

when we set, we'll need to add a level and flag if we want entry, final_entry or both to be set with the tag. (probably that must be array with level and flag)
the processing will be going 

default tag will always come as option.

we can imagine we want to have a 'module-specific' map
that has:
1 or 2 levels, and null default tag:
1. members and args of functions
2. (for most cases we may be ok to combing thiw with 1) final entries
anyway when the metac_tag_map_tag works it will try final as the latest option if it sees a flag in the mask for this level.


if for some reason the module-specifc map doesn't work should be easy to create our own. Or
to create it partially and link the second layer to the original and have a default if needed.
it's better alsoway have null default tag on module-specific map.
 */

/** @brief value object declaraion (hidden implementation )*/
typedef struct metac_value metac_value_t;

/* event for callback */

/** @brief event types from deep functions (when deep function needs some insight on what to do) */
typedef enum {
    METAC_RQVST_union_member = 1,   /**< return value union member to continue */
    METAC_RQVST_flex_array_count,   /**< return metac_new_element_count_value of flex array with proper len */
    METAC_RQVST_pointer_array_count,/**< return metac_new_element_count_value of pointer with proper len */
    METAC_RQVST_va_list,            /**< return */
}metac_value_event_type_t;

/** @brief event structure which includes type and plase where the hanlder needs to put return value */
typedef struct{
    metac_value_event_type_t type;                  /**< type of event*/
    metac_value_t *p_return_value;                  /**< for METAC_RQVST_union_member, 
                                                        METAC_RQVST_flex_array_count and METAC_RQVST_pointer_array_count.
                                                        METAC_RQVST_va_list uses it to pass param_storage to the callback*/
    metac_num_t va_list_param_id;                   /**< get from here the id of va_arg in case METAC_RQVST_va_list*/
    struct va_list_container * p_va_list_container; /**< pointer to va_list to handle case METAC_RQVST_va_list*/
}metac_value_event_t;

typedef struct metac_value_walker_hierarchy metac_value_walker_hierarchy_t;
typedef int (*metac_value_event_handler_t)(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context);

/** @brief tag which can be assigned to metac_entry in tagmap */
typedef struct metac_entry_tag {
    char * p_tagstring; /**<  go-like (https://pkg.go.dev/reflect#StructTag) taglist - 
                            it's always dynamically allocated (We make a copy of it)*/

    /*
        extra callback support for c-specific things - maybe we can code that as test later.. e.g. `json:",omitempty" yaml:",omitempty"` =>
        "json:\",omitempty\" yaml:\",omitempty\" metac:\"count_by(another field)\"" though text is pretty limited 
    */
    void *p_context; /**< some arbitrary data which will be passed to the handler as last argument */
    metac_value_event_handler_t handler; /**< handler which may provide infomration to deep functions */
    void (*context_free_handler)(void *p_context); /**< function which will free p_context on destroy, because it can be complex */

}metac_entry_tag_t;

/** @brief tag map declaration - implementation is hidden */
typedef struct metac_tag_map metac_tag_map_t;

/** @brief tagmap entry categories */
typedef enum {
    METAC_TEC_variable = 0,         /**< corresponds to METAC_KND_variable */
    METAC_TEC_func_parameter, /**< corresponds to METAC_KND_func_parameter*/
    METAC_TEC_member,               /**< corresponds to METAC_KND_member */
    METAC_TEC_final,                /**< can be base_type, enum, pointer, array, structure/union/class)*/
}metac_tag_map_entry_category_t;

/** @brief tagmap entry categories mask (need just 4 bits ) */
typedef uint8_t metac_tag_map_entry_category_mask_t;

/** @brief use metac_tag_map_entry_category_t as x*/
#define METAC_TAG_MAP_ENTRY_CATEGORY_MASK(x) (1 << x)

/** @brief tagmap can consist of several metac_tag_map_entry_t */
typedef struct {
    metac_tag_map_entry_category_mask_t mask;   /**< category mask for which this particular map is valid */
    metac_tag_map_t * p_ext_map;    /**< if external map - put here pointer. otherwise NULL */
}metac_tag_map_entry_t;

/** @brief possible operations of assignment of the tag */
typedef enum {
    METAC_TEO_entry = 1,    /**< use this operation to assign tag to the entry itself*/
    METAC_TEO_final_entry,  /**< use this operation to assign tag to the final entry of the entry provided */
    // can't do both, because in that case we'll have 2 times the same context for metac_handler - not sure how to delete
}metac_tag_map_op_t;

/**
 * @brief creates tagmap with given parameters
 * @param categories_count number of categories planned in this tagmap - length for the next parameter
 * @param p_map_entries map entries - mask + pointer to the external tagmap
 * @param p_default_tag can be NULL. if set - tagmap will return it if nothing is found for the entry
*/
metac_tag_map_t * metac_new_tag_map_ex(metac_num_t categories_count, metac_tag_map_entry_t * p_map_entries, metac_entry_tag_t *p_default_tag);
int metac_tag_map_set_tag(metac_tag_map_t * p_tag_map, 
    metac_num_t category,
    metac_flag_t override,
    metac_entry_t* p_entry,
    metac_tag_map_op_t op,
    metac_entry_tag_t *p_tag);

/**
 * @brief lookup for tag by entry pointer
 * @param p_tag_map - configured tagmap
 * @param p_entry - pointer to the entry
 * @return the tag assigned
*/
metac_entry_tag_t * metac_tag_map_tag(metac_tag_map_t * p_tag_map, metac_entry_t* p_entry);

/**
 * @brief cleanup tagmap memory
*/
void metac_tag_map_delete(metac_tag_map_t * p_tag_map);

/** 
 * @brief creates tagmap with 1 category
 *      with mask =
 *          METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_variable) | 
 *          METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_func_parameter) | 
 *          METAC_TAG_MAP_ENTRY_CATEGORY_MASK(METAC_TEC_member);
 */
metac_tag_map_t * metac_new_tag_map(metac_entry_tag_t *p_default_tag);

/* new set of macroses for ARGS, set of macroses for 'final' types */
/** 
 * @brief start new tagmap generator function 
 */
#define METAC_TAG_MAP_NEW(_name_, _default_tag_, _map_entries_...) metac_tag_map_t * _name_() { \
        metac_flag_t append = 0; \
        metac_tag_map_entry_t map_entries[] = {_map_entries_}; \
        metac_tag_map_t * p_map = metac_new_tag_map_ex(sizeof(map_entries)/sizeof(map_entries[0]), map_entries, _default_tag_); \
        if (p_map == NULL) { \
            return NULL; \
        }
/**
 * @brief start new tagmap append-generator function (which appends the existing tagmap)
*/
#define METAC_TAG_MAP_APPEND(_name_) metac_tag_map_t * _name_(metac_tag_map_t * p_map) { \
        metac_flag_t append = 1; \
        if (p_map == NULL) { \
            return NULL; \
        }
/**
 * @brief end the tagmap (append-)generator function
*/
#define METAC_TAG_MAP_END \
        return p_map; \
    }

/** @brief select entry for subroutines and global variables via link */
#define METAC_TAG_MAP_ENTRY(_entry_) do { \
        metac_entry_t *p_entry = _entry_; \
        if (p_entry == NULL) { \
            if (append == 0) { \
                metac_tag_map_delete(p_map); \
            } \
            return NULL; \
        }
/** @brief select entry of type by type name */
#define METAC_TAG_MAP_ENTRY_FROM_TYPE(_type_) do { \
        WITH_METAC_DECLLOC(loc, _type_ * val); \
        metac_entry_t *p_entry = metac_entry_final_entry(metac_entry_pointer_entry(METAC_ENTRY_FROM_DECLLOC(loc, val)), NULL); \
        if (p_entry == NULL) { \
            if (append == 0) { \
                metac_tag_map_delete(p_map); \
            } \
            return NULL; \
        }
/** @brief close METAC_TAG_MAP_ENTRY section */
#define METAC_TAG_MAP_ENTRY_END \
    } while(0);

/** @brief use the entry which was seleted by METAC_TAG_MAP_ENTRY... */
#define METAC_TAG_MAP_ENTRY_SELF p_entry
/** @brief use the member of the entry which was seleted by METAC_TAG_MAP_ENTRY... */
#define METAC_TAG_MAP_ENTRY_MEMBER(_expression_...) ({ \
        metac_entry_id_t id[] = {_expression_}; \
        metac_entry_by_member_ids(p_entry, 0, id, sizeof(id)/sizeof(id[0])); \
    })
/** @brief use the arguments of the entry which was seleted by METAC_TAG_MAP_ENTRY... */
#define METAC_TAG_MAP_ENTRY_PARAMETER(_expression_...) \
        metac_entry_by_parameter_ids(p_entry, 0, (metac_entry_id_t[]){_expression_})

/** @brief helper to include tag into parameters */
#define METAC_TAG(_fields_...) (metac_entry_tag_t[]){{_fields_}}
/**
 * @brief set tag for the selected entry
 * @param _cat_ tagmap category where this tag will be added
 * @param _op_ entry itself, or its filnal entry. see metac_tag_map_op_t
 * @param _override_ 0 or 1. if 0 and tag already exists - the function will fail
 * @param _entry_ entry for each the tag will be added. Possible options are:
 *  METAC_TAG_MAP_ENTRY_SELF - means that the entry set by METAC_TAG_MAP_ENTRY... will be used
 *  METAC_TAG_MAP_ENTRY_MEMBER - for METAC_TAG_MAP_ENTRY... which are structures/unions - it's possible to select the field in the hierarchy for which the tag will be assigned
 *  METAC_TAG_MAP_ENTRY_PARAMETER - for METAC_TAG_MAP_ENTRY... which are subroutines - tag for function argumetns.
 * @param _tag_fields_ - tags: use METAC_TAG_STRING/METAC_TAG_QSTRING for string tags,
 *  METAC_COUNT_BY, METAC_ZERO_ENDED_STRING, METAC_CONTAINER_OF, METAC_UNION_MEMBER_SELECT_BY, METAC_CAST_PTR (this can be extended by user) or combination
*/
#define METAC_TAG_MAP_SET_TAG(_cat_, _op_, _override_, _entry_, _tag_fields_...) \
        if (metac_tag_map_set_tag(p_map, _cat_, _override_, _entry_, _op_, METAC_TAG(_tag_fields_)) != 0) { \
            if (append == 0) { \
                metac_tag_map_delete(p_map); \
            } \
            return NULL; \
        }

/** 
 * @brief set stringtag using this
 * @param _string_ - string in quotes, e.g. "test"
 */
#define METAC_TAG_STRING(_string_) \
    .p_tagstring = _string_,
/** 
 * @brief set stringtag using this - allows to avoid quotes handling and should be more convenient
 * @param _string_ - string without quotes, e.g. test, "some text", anything... will be converted to  "test, /"some text/", anything...""
 */
#define METAC_TAG_QSTRING(_string_...) \
    METAC_TAG_STRING(#_string_)

/**
 * @brief function to extract subtag from string tag by key. similar to https://pkg.go.dev/reflect#StructTag.Lookup
*/
metac_name_t metac_entry_tag_string_lookup(metac_entry_tag_t *p_tag, metac_name_t in_key);

/* some reference handlers */
/**
 * @brief link flexible array or pointer in the struct with the field OR in the function with the parameter that provides its length
 * @param _fld_ sibling fieldname where length is located
*/
#define METAC_COUNT_BY(_fld_) \
    .handler = metac_handle_count_by, \
    .context_free_handler = metac_count_by_context_delete, \
    .p_context = metac_new_count_by_context(#_fld_), 

struct metac_count_by_context {
    metac_name_t counter_sibling_entry_name; // easily can be covered as text tag
};
struct metac_count_by_context * metac_new_count_by_context(metac_name_t counter_sibling_entry_name);
void metac_count_by_context_delete(void *p_context);
int metac_handle_count_by(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context);

/**
 * @brief mark pointer or flexible array field as null-terminated string so the length of will be calculated as strlen + 1
 * should be used only for char* and char[]
*/
#define METAC_ZERO_ENDED_STRING() \
    .handler = metac_handle_zero_ended_string,
/** @brief internal implementation */
int metac_handle_zero_ended_string(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context);

/**
 * @brief instruct deep functions that this field contains pointer which points to the member of the _container_type_
 * @param _container_type_ - the whole structure
 * @param _member_ to which the pointer field points
*/
#define METAC_CONTAINER_OF(_container_type_, _member_) \
    .handler = metac_handle_container_of, \
    .context_free_handler = metac_container_of_context_delete, \
    .p_context = metac_new_container_of_context( \
        ((metac_size_t)&(((_container_type_ *)0)->_member_)), \
        ({ WITH_METAC_DECLLOC(loc, _container_type_ * p = NULL; METAC_ENTRY_FROM_DECLLOC(loc, p));})),
/** @brief internal implementation */
struct metac_container_of_context {
    metac_size_t member_offset;
    metac_entry_t * p_coontainer_entry;
};
/** @brief internal implementation */
struct metac_container_of_context * metac_new_container_of_context(metac_size_t member_offset,
    metac_entry_t * p_coontainer_entry);
/** @brief internal implementation */
void metac_container_of_context_delete(void *p_context);
/** @brief internal implementation */
int metac_handle_container_of(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context);

/**
 * @brief instruct deep functions how to choose which field is actual now
 * @param _fld_ - sibling fieldname which is necessary to analyse.  
 * @param _cases_ - optional. if missed the _fld_ value will be converted to the union field number (started from 0)
 *  can provide pairs {.fld_val = somevalue, union_fld_name = "fieldname"} OR {.fld_val = somevalue, union_fld_id = union field number started from 0}
 *  separated with comma.
*/
#define METAC_UNION_MEMBER_SELECT_BY(_fld_, _cases_...) \
    .handler = metac_handle_union_member_select_by, \
    .context_free_handler = metac_union_member_select_by_context_delete, \
    .p_context = metac_new_union_member_select_by_context(#_fld_, \
        (struct metac_union_member_select_by_case[]){_cases_}, \
        sizeof((struct metac_union_member_select_by_case[]){_cases_})),
/** @brief match value to the field name or id */
struct metac_union_member_select_by_case {
    metac_num_t fld_val;
    metac_name_t union_fld_name;
    metac_num_t union_fld_id;   /* id is used when name is NULL*/
};
/** @brief internal implementation */
struct metac_union_member_select_by_context {
    metac_name_t sibling_selector_fieldname; // easily can be covered as text tag
    metac_num_t cases_count;    /* if count is 0 using 1-to-1 default approach - field value is id of union memeber */
    struct metac_union_member_select_by_case *p_cases;
};
/** @brief internal implementation */
struct metac_union_member_select_by_context * metac_new_union_member_select_by_context(metac_name_t sibling_fieldname,
    struct metac_union_member_select_by_case * p_cases, size_t case_sz);
void metac_union_member_select_by_context_delete(void *p_context);
/** @brief internal implementation */
int metac_handle_union_member_select_by(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context);

/**
 * @brief instruct deep functions to cast pointer to the array sor single element of the type based on the value of _selector_fld_ and optionally _count_fld_
 * @param _count_fld_ - optional (can be empty) sibling fieldname which contains information about the array length. if empty len == 1.  
 * @param _selector_fld_ - sibling fieldname which contains value to analyse based on cases.  
 * @param _cases_ - contains list of cases separated with comma. each case is a pair
 *  {.fld_val = some value, .p_ptr_entry = entry}. to get entry from type it's possible to use METAC_ENTRY_OF(_type_)
*/
#define METAC_CAST_PTR(_count_fld_, _selector_fld_, _cases_...) \
    .handler = metac_handle_ptr_cast, \
    .context_free_handler = metac_ptr_cast_context_delete, \
    .p_context = metac_new_ptr_cast_context(#_count_fld_,  #_selector_fld_, \
        (struct metac_ptr_cast_case[]){_cases_}, \
        sizeof((struct metac_ptr_cast_case[]){_cases_})), 

/**
 * @brief get metac_entry_t for the given type. used in combination with METAC_CAST_PTR or separatelly
 * @param _type_ name of type
*/
#define METAC_ENTRY_OF(_type_) ({ WITH_METAC_DECLLOC(loc, _type_ p = NULL; metac_entry_final_entry(METAC_ENTRY_FROM_DECLLOC(loc, p), NULL));})

/** @brief information about pair: value -> metac_entry_t */
struct metac_ptr_cast_case {
    metac_num_t fld_val;
    metac_entry_t * p_ptr_entry;    // new type
};
/** @brief internal implementation */
struct metac_ptr_cast_context {
    metac_name_t sibling_count_fieldname;   // if NULL - 1 will be used by default
    metac_name_t sibling_selector_fieldname; // mandatory field
    metac_num_t cases_count;    /* if count is 0 using 1-to-1 default approach - field value is id of union memeber */
    struct metac_ptr_cast_case *p_cases;
};
/** @brief internal implementation */
struct metac_ptr_cast_context * metac_new_ptr_cast_context(
    metac_name_t sibling_count_fieldname,
    metac_name_t sibling_selector_fieldname,
    struct metac_ptr_cast_case * p_cases, size_t case_sz);
/** @brief internal implementation */
void metac_ptr_cast_context_delete(void *p_context);
/** @brief internal implementation */
int metac_handle_ptr_cast(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context);

/**
 * @brief instruct metac_new_value_with_parameters or metac_new_value_with_vparameters that the argument is variadic and
 * can be handled based on the previous argument that is pritnf format string
*/
#define METAC_FORMAT_BASED_VA_ARG() \
    .handler = metac_handle_printf_format,

/** @brief internal implementation */
int metac_handle_printf_format(metac_value_walker_hierarchy_t *p_hierarchy, metac_value_event_t * p_ev, void *p_context);

#endif