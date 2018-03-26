/*
 * metac_precompiled_type.c
 *
 *  Created on: Nov 28, 2017
 *      Author: mralex
 */

#include "metac_type.h"
#include "metac_precompiled_type_int.h"	/*definitions of internal objects*/
#include "metac_debug.h"	/* msg_stderr, ...*/
#include "breadthfirst_engine.h" /*breadthfirst_engine module*/

#include <stdlib.h>			/* calloc, ... */
#include <string.h>			/* strlen, strcpy */
#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <urcu/list.h>		/* I like struct cds_list_head :) */

#ifndef cds_list_for_each_safe
/*some versions of urcu don't have cds_list_for_each_safe, but it will be ok to delete elements in reverse direction in our case*/
#define cds_list_for_each_safe cds_list_for_each_prev_safe
#endif

/*****************************************************************************/
/*temporary types for precompilation*/
/*****************************************************************************/
struct _discriminator {
	struct cds_list_head list;

	struct discriminator * p_discriminator;
};

struct _region_type {
	struct cds_list_head list;

	struct region_type * p_region_type;
	struct cds_list_head discriminator_list;
};

struct precompile_context {
	metac_precompiled_type_t * precompiled_type;

	struct cds_list_head region_type_list;	/*current list of all created region types for precompiled type */
};

struct precompile_task {
	struct breadthfirst_engine_task task;

	struct precompile_task* parent_task;
	struct _region_type * _region_type;
	struct metac_type *type;
	struct condition precondition;
	char *	name_local;
	char *	given_name_local;
	metac_data_member_location_t offset;
	metac_byte_size_t byte_size;

	/* runtime data */
	struct metac_type *actual_type;
	struct region_type_element * region_type_element;
};
/*****************************************************************************/
static int delete_discriminator(struct discriminator ** pp_discriminator) {
	if (pp_discriminator == NULL) {
		msg_stderr("Can't delete discriminator: invalid parameter\n");
		return -EINVAL;
	}

	if (*pp_discriminator == NULL) {
		msg_stderr("Can't delete discriminator: already deleted\n");
		return -EALREADY;
	}

	free(*pp_discriminator);
	*pp_discriminator = NULL;

	return 0;
}

static struct discriminator * create_discriminator(
		struct discriminator * p_previous_discriminator,
		metac_discriminator_value_t previous_expected_discriminator_value,
		metac_discriminator_cb_ptr_t discriminator_cb,
		void * discriminator_cb_context
		) {
	struct discriminator * p_discriminator;
	p_discriminator = calloc(1, sizeof(*(p_discriminator)));
	if (p_discriminator == NULL) {
		msg_stderr("Can't create discriminator: no memory\n");
		return NULL;
	}

	if (p_previous_discriminator != NULL) {	/*copy precondition*/
		p_discriminator->precondition.p_discriminator = p_previous_discriminator;
		p_discriminator->precondition.expected_discriminator_value = previous_expected_discriminator_value;
	}

	p_discriminator->discriminator_cb = discriminator_cb;
	p_discriminator->discriminator_cb_context = discriminator_cb_context;

	return p_discriminator;
}
/*****************************************************************************/
static int delete_region_type_element(struct region_type_element **pp_region_type_element) {
	struct region_type_element *p_region_type_element;

	if (pp_region_type_element == NULL) {
		msg_stderr("Can't delete region_type: invalid parameter\n");
		return -EINVAL;
	}

	p_region_type_element = *pp_region_type_element;
	if (p_region_type_element == NULL) {
		msg_stderr("Can't delete region_type: already deleted\n");
		return -EALREADY;
	}

	if (p_region_type_element->name_local != NULL) {
		free(p_region_type_element->name_local);
		p_region_type_element->name_local = NULL;
	}
	if (p_region_type_element->path_within_region != NULL) {
		free(p_region_type_element->path_within_region);
		p_region_type_element->path_within_region = NULL;
	}
	if (p_region_type_element->path_global != NULL) {
		free(p_region_type_element->path_global);
		p_region_type_element->path_global = NULL;
	}
	free(p_region_type_element);
	*pp_region_type_element = NULL;

	return 0;
}

static void update_region_type_element_array_params(
		struct region_type_element *p_region_type_element,
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr,
		void *	array_elements_count_cb_context,
		struct region_type * array_elements_region_type) {
	p_region_type_element->array_elements_count_funtion_ptr = array_elements_count_funtion_ptr;
	p_region_type_element->array_elements_count_cb_context = array_elements_count_cb_context;
	p_region_type_element->array_elements_region_type = array_elements_region_type;
}

static struct region_type_element * create_region_type_element(
		struct metac_type * type,
		struct discriminator * p_discriminator,
		metac_discriminator_value_t expected_discriminator_value,
		metac_data_member_location_t offset,
		metac_byte_size_t byte_size,
		struct region_type_element * parent,
		char *	name_local,
		char *	path_within_region,
		char *	path_global,
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr,
		void *	array_elements_count_cb_context,
		struct region_type * array_elements_region_type) {

	struct region_type_element *p_region_type_element;

	if (type == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	msg_stddbg("(name_local = %s, path_within_region = %s, path_global = %s, offset = %d, byte_size = %d)\n",
			name_local, path_within_region, path_global,
			(int)offset, (int)byte_size);

	p_region_type_element = calloc(1, sizeof(*(p_region_type_element)));
	if (p_region_type_element == NULL) {
		msg_stderr("Can't create region_type_element: no memory\n");
		return NULL;
	}

	p_region_type_element->type = type;

	if (p_discriminator != NULL) {	/*copy precondition*/
		p_region_type_element->precondition.p_discriminator = p_discriminator;
		p_region_type_element->precondition.expected_discriminator_value = expected_discriminator_value;
	}

	p_region_type_element->offset = offset;
	p_region_type_element->byte_size = byte_size;

	p_region_type_element->parent = parent;

	p_region_type_element->name_local = (name_local != NULL)?strdup(name_local):NULL;
	p_region_type_element->path_within_region = (path_within_region != NULL)?strdup(path_within_region):NULL;
	p_region_type_element->path_global = (path_global != NULL)?strdup(path_global):NULL;;
	if (	(name_local != NULL && p_region_type_element->name_local == NULL) ||
			(path_within_region != NULL && p_region_type_element->path_within_region == NULL) ||
			(path_global != NULL && p_region_type_element->path_global == NULL) ) {
		delete_region_type_element(&p_region_type_element);
		return NULL;
	}

	update_region_type_element_array_params(p_region_type_element,
			array_elements_count_funtion_ptr, array_elements_count_cb_context, array_elements_region_type);

	return p_region_type_element;
}
/*****************************************************************************/
static int delete_region_type(struct region_type ** pp_region_type) {
	int i;
	struct region_type *p_region_type;

	if (pp_region_type == NULL) {
		msg_stderr("Can't delete region_type: invalid parameter\n");
		return -EINVAL;
	}

	p_region_type = *pp_region_type;
	if (p_region_type == NULL) {
		msg_stderr("Can't delete region_type: already deleted\n");
		return -EALREADY;
	}

	for (i = 0; i < p_region_type->elements_count; i++) {
		delete_region_type_element(&p_region_type->element[i]);
	}
	free(p_region_type->element);
	p_region_type->element = NULL;
	free(p_region_type->base_type_element);
	p_region_type->base_type_element = NULL;
	free(p_region_type->enum_type_element);
	p_region_type->enum_type_element = NULL;
	free(p_region_type->pointer_type_element);
	p_region_type->pointer_type_element = NULL;
	free(p_region_type->array_type_element);
	p_region_type->array_type_element = NULL;
	free(p_region_type->hierarchy_element);
	p_region_type->hierarchy_element = NULL;

	for (i = 0; i < p_region_type->discriminators_count; i++) {
		delete_discriminator(&p_region_type->discriminator[i]);
	}
	free(p_region_type->discriminator);
	p_region_type->discriminator = NULL;


	free(p_region_type);
	*pp_region_type = NULL;

	return 0;
}

static struct region_type * create_region_type(
		struct metac_type * type) {

	struct region_type *p_region_type;

	if (type == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	msg_stddbg("create_region_type: %s\n", type->name);

	p_region_type = calloc(1, sizeof(*(p_region_type)));
	if (p_region_type == NULL) {
		msg_stderr("Can't create region_type: no memory\n");
		return NULL;
	}

	p_region_type->type = type;

	p_region_type->discriminators_count = 0;
	p_region_type->discriminator = NULL;

	p_region_type->elements_count = 0;
	p_region_type->element = NULL;

	p_region_type->hierarchy_element = NULL;
	p_region_type->hierarchy_elements_count = 0;
	p_region_type->base_type_element = NULL;
	p_region_type->base_type_elements_count = 0;
	p_region_type->enum_type_element = NULL;
	p_region_type->enum_type_elements_count = 0;
	p_region_type->pointer_type_element = NULL;
	p_region_type->pointer_type_elements_count = 0;
	p_region_type->array_type_element = NULL;
	p_region_type->array_type_elements_count = 0;

	return p_region_type;
}
/*****************************************************************************/
static int delete_precompiled_type(struct metac_precompiled_type ** pp_precompiled_type) {
	int i;
	struct metac_precompiled_type *p_precompiled_type;

	if (pp_precompiled_type == NULL) {
		msg_stderr("Can't delete precompiled_type: invalid parameter\n");
		return -EINVAL;
	}

	p_precompiled_type = *pp_precompiled_type;
	if (p_precompiled_type == NULL) {
		msg_stderr("Can't delete precompiled_type: already deleted\n");
		return -EALREADY;
	}

	for (i = 0; i < p_precompiled_type->region_types_count; i++) {
		delete_region_type(&p_precompiled_type->region_type[i]);
	}

	if (p_precompiled_type->region_type != NULL) {
		free(p_precompiled_type->region_type);
		p_precompiled_type->region_type = NULL;
	}

	free(p_precompiled_type);
	*pp_precompiled_type = NULL;

	return 0;
}

static struct metac_precompiled_type * create_precompiled_type(
		struct metac_type * type) {

	struct metac_precompiled_type *p_precompiled_type;

	if (type == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	p_precompiled_type = calloc(1, sizeof(*(p_precompiled_type)));
	if (p_precompiled_type == NULL) {
		msg_stderr("Can't create precompiled_type: no memory\n");
		return NULL;
	}

	p_precompiled_type->type = type;

	p_precompiled_type->region_types_count = 0;
	p_precompiled_type->region_type = NULL;

	return p_precompiled_type;
}
/*****************************************************************************/
static struct _region_type * create__region_type(
		struct metac_type * type) {
	struct _region_type * _region_type;

	_region_type = calloc(1, sizeof(*_region_type));
	if (_region_type == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	_region_type->p_region_type = create_region_type(type);
	if (_region_type->p_region_type == NULL) {
		msg_stderr("create_region_type failed\n");
		free(_region_type);
		return NULL;
	}
	CDS_INIT_LIST_HEAD(&_region_type->discriminator_list);

	return _region_type;
}

static struct _region_type * find_or_create_region_type(/*struct cds_list_head *p_region_type_list,*/
		struct precompile_context * p_precompile_context,
		struct metac_type * type) {
	/*check if region_type for the same type already exists*/
	struct _region_type * _region_type = NULL;
	struct _region_type * _region_type_iter;

	cds_list_for_each_entry(_region_type_iter, &p_precompile_context->region_type_list, list) {
		if (_region_type_iter->p_region_type->type == type){
			_region_type = _region_type_iter;
			msg_stddbg("found %p\n", _region_type);
		}
	}
	if (_region_type == NULL) {
		/*create otherwise*/
		msg_stddbg("create region_type for : %s\n", type->name);
		_region_type = create__region_type(type);
		msg_stddbg("create region_type result %p\n", _region_type);
		if (_region_type == NULL) {
			msg_stddbg("create__region_type failed\n");
			return NULL;
		}
		cds_list_add_tail(&_region_type->list, &p_precompile_context->region_type_list);
		++p_precompile_context->precompiled_type->region_types_count;
	}

	return _region_type;
}
/*****************************************************************************/
static struct _discriminator * create__discriminator(
		struct _region_type * _region_type,
		struct discriminator * p_previous_discriminator,
		metac_discriminator_value_t previous_expected_discriminator_value,
		metac_discriminator_cb_ptr_t discriminator_cb,
		void * discriminator_cb_context
		) {
	struct _discriminator * _discriminator;

	_discriminator = calloc(1, sizeof(*_discriminator));
	if (_discriminator == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	_discriminator->p_discriminator = create_discriminator(p_previous_discriminator,
			previous_expected_discriminator_value, discriminator_cb, discriminator_cb_context);
	if (_discriminator->p_discriminator == NULL) {
		msg_stderr("create_discriminator failed\n");
		free(_discriminator);
		return NULL;
	}

	cds_list_add(&_discriminator->list, &_region_type->discriminator_list);
	++_region_type->p_region_type->discriminators_count;
	return _discriminator;
}
/*****************************************************************************/
static int delete_precompile_task(struct precompile_task ** pp_ptask) {
	struct precompile_task * p_ptask;

	if (pp_ptask == NULL) {
		msg_stderr("Can't delete ptask: invalid parameter\n");
		return -EINVAL;
	}

	p_ptask = *pp_ptask;
	if (p_ptask == NULL) {
		msg_stderr("Can't delete ptask: already deleted\n");
		return -EALREADY;
	}

	if (p_ptask->name_local) {
		free(p_ptask->name_local);
		p_ptask->name_local = NULL;
	}
	if (p_ptask->given_name_local) {
		free(p_ptask->given_name_local);
		p_ptask->given_name_local = NULL;
	}

	free(p_ptask);
	*pp_ptask = NULL;

	return 0;
}

static struct precompile_task* create_and_add_precompile_task(
		struct breadthfirst_engine* p_breadthfirst_engine,
		struct precompile_task * parent_task,
		struct _region_type * _region_type,
		struct metac_type * type,
		breadthfirst_engine_task_fn_t fn,
		breadthfirst_engine_task_destructor_t destroy,
		struct discriminator * p_discriminator,
		metac_discriminator_value_t expected_discriminator_value,
		char * name_local,
		char * given_name_local,
		metac_data_member_location_t offset,
		metac_byte_size_t byte_size) {
	struct precompile_task* ptask;

	msg_stddbg("(name_local = %s, given_name_local = %s, offset = %d, byte_size = %d)\n",
			name_local, given_name_local,
			(int)offset, (int)byte_size);

	/* allocate object */
	ptask = calloc(1, sizeof(*ptask));
	if (ptask == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	ptask->task.fn = fn;
	ptask->task.destroy = destroy;

	ptask->parent_task = parent_task;
	ptask->_region_type = _region_type;
	ptask->type = type;
	ptask->name_local = name_local!=NULL?strdup(name_local):NULL;
	ptask->given_name_local = given_name_local!=NULL?strdup(given_name_local):NULL;
	ptask->offset = offset;
	ptask->byte_size = byte_size;

	if (p_discriminator != NULL) {	/*copy precondition*/
		ptask->precondition.p_discriminator = p_discriminator;
		ptask->precondition.expected_discriminator_value = expected_discriminator_value;
	}

	if (add_breadthfirst_task(p_breadthfirst_engine, &ptask->task) != 0) {
		msg_stderr("add_breadthfirst_task failed\n");
		free(ptask);
		return NULL;
	}
	return ptask;
}
/*****************************************************************************/
/* similar to metac_type_typedef_skip, but skips more types (like constant and etc ) */
static struct metac_type *get_actual_type(struct metac_type *type) {
	if (type == NULL){
		msg_stderr("invalid argument value: return NULL\n");
		return NULL;
	}
	if (	type->id == DW_TAG_typedef ||
			type->id == DW_TAG_const_type) {
		if (type->typedef_info.type == NULL) {
			msg_stderr("typedef/const_type has to contain type in attributes: return NULL\n");
			return NULL;
		}
		return get_actual_type((type->id == DW_TAG_typedef)?(type->typedef_info.type):(type->const_type_info.type));
	}
	return type;
}

static char * build_path(char * parent_path, char * name_local){
	char * result;
	size_t size1;
	size_t size2;

	if (parent_path == NULL)parent_path = "";
	if (name_local == NULL)name_local = "";

	size1 = strlen(parent_path);
	size2 = strlen(name_local);
	result = calloc(sizeof(char), size1 + (size_t)((size1 > 0)?1:0) + size2 + (size_t)1);
	if (result == NULL){
		return NULL;
	}
	if (size1 > 0){
		strcpy(result, parent_path);
		strcpy(result + size1, ".");
	}
	strcpy(result + size1 + (size_t)((size1 > 0)?1:0), name_local);
	return result;
}

/*****************************************************************************/
static int _parse_type_task_destroy(
	struct breadthfirst_engine * p_breadthfirst_engine,
	struct breadthfirst_engine_task * p_breadthfirst_engine_task) {
	struct precompile_task * p_precompile_task = cds_list_entry(p_breadthfirst_engine_task, struct precompile_task, task);
	delete_precompile_task(&p_precompile_task);
	return 0;
}

static int _parse_type_task(
		struct breadthfirst_engine * p_breadthfirst_engine,
		struct breadthfirst_engine_task * p_breadthfirst_engine_task){
	/*TODO: check spec on global level - e.g. that will allow to make Stop for some type*/
	char * path_global;
	char * path_within_region;
	char * parent_path_global;
	char * parent_path_within_region;
	struct precompile_task * p_precompile_task = cds_list_entry(p_breadthfirst_engine_task, struct precompile_task, task);
	struct precompile_context * p_precompile_context = (struct precompile_context *)p_breadthfirst_engine->private_data;
	if (p_precompile_context == NULL) {
		msg_stderr("breadthfirst_engine_2_precompile_context failed\n");
		return -EINVAL;
	}

	/* get actual type */
	p_precompile_task->actual_type = get_actual_type(p_precompile_task->type);

	/* generate paths */
	if (	p_precompile_task->parent_task != NULL &&
			p_precompile_task->parent_task->region_type_element != NULL &&
			p_precompile_task->parent_task->region_type_element->path_global != NULL) {
		parent_path_global = p_precompile_task->parent_task->region_type_element->path_global;
		parent_path_within_region = p_precompile_task->parent_task->_region_type == p_precompile_task->_region_type?
				p_precompile_task->parent_task->region_type_element->path_within_region:
				"";
	}else{
		parent_path_global = "";
		parent_path_within_region = "";
	}

	path_global = build_path(parent_path_global, p_precompile_task->given_name_local);
	if (path_global == NULL) {
		msg_stderr("build path for path_global\n");
		return -EFAULT;
	}
	path_within_region = build_path(parent_path_within_region, p_precompile_task->given_name_local);
	if (path_within_region == NULL) {
		free(path_global);
		msg_stderr("build path for path_within_region\n");
		return -EFAULT;
	}

	/*create struct region_type_element in our region_type based on the data from task*/
	p_precompile_task->region_type_element = create_region_type_element(
			p_precompile_task->type,
			p_precompile_task->precondition.p_discriminator, p_precompile_task->precondition.expected_discriminator_value,
			p_precompile_task->offset, p_precompile_task->byte_size,
			p_precompile_task->parent_task != NULL?p_precompile_task->parent_task->region_type_element:NULL,
			p_precompile_task->name_local, path_within_region, path_global, NULL, NULL, NULL);

	free(path_global);
	free(path_within_region);

	if (p_precompile_task->region_type_element == NULL) {
		msg_stddbg("failed to create region_type_element\n");
		return -EFAULT;
	}

	/* generate children tasks */
	switch(p_precompile_task->actual_type->id) {
	case DW_TAG_structure_type: {
		metac_type_t * type = p_precompile_task->actual_type;
		int is_anon;
		int anon_members_count = 0;
		metac_num_t i;
		for (i = 0; i < type->structure_type_info.members_count; i++) {
			char anon_name[15];
			is_anon = 0;
			if (strcmp(type->structure_type_info.members[i].name, "") == 0) {
				is_anon = 1;
				snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
			}
			if (create_and_add_precompile_task(
					p_breadthfirst_engine,
					p_precompile_task,
					p_precompile_task->_region_type,
					type->structure_type_info.members[i].type,
					_parse_type_task,
					_parse_type_task_destroy,
					p_precompile_task->precondition.p_discriminator, p_precompile_task->precondition.expected_discriminator_value,
					type->structure_type_info.members[i].name,
					is_anon?anon_name:type->structure_type_info.members[i].name,
					p_precompile_task->offset + type->structure_type_info.members[i].data_member_location,
					metac_type_byte_size(type->structure_type_info.members[i].type)) == NULL) {
				msg_stderr("create_and_add_precompile_task failed\n");
				return -EFAULT;
			}
		}
	} break;
	case DW_TAG_union_type: {
		metac_type_t * type = p_precompile_task->actual_type;
		struct _discriminator * _discriminator;
		int is_anon;
		int anon_members_count = 0;
		metac_num_t i;
		/* try to find discriminator ptr */
		const metac_type_specification_value_t * spec =
				metac_type_specification(p_precompile_context->precompiled_type->type, p_precompile_task->region_type_element->path_global);
		/*TODO: we can check p_precompile_task->type */
		if (spec == NULL || spec->discriminator_funtion_ptr == NULL) {
			msg_stddbg("Warning: Union %s doesn't have a union-type specification - skipping its children\n",
					p_precompile_task->region_type_element->path_global);
			/*TODO: mark to skip the step */
			break;
		}
		/*we have new discriminator in spec - let's create it*/
		_discriminator = create__discriminator(p_precompile_task->_region_type,
				p_precompile_task->precondition.p_discriminator, p_precompile_task->precondition.expected_discriminator_value,
				spec->discriminator_funtion_ptr,
				spec->specification_context);
		if (_discriminator == NULL) {
			return -EFAULT;
		}

		for (i = 0; i < type->union_type_info.members_count; i++) {
			char anon_name[15];

			is_anon = 0;
			if (strcmp(type->union_type_info.members[i].name, "") == 0) {
				is_anon = 1;
				snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
			}
			if (create_and_add_precompile_task(
					p_breadthfirst_engine,
					p_precompile_task,
					p_precompile_task->_region_type,
					type->structure_type_info.members[i].type,
					_parse_type_task,
					_parse_type_task_destroy,
					_discriminator->p_discriminator, i,
					type->structure_type_info.members[i].name,
					is_anon?anon_name:type->structure_type_info.members[i].name,
					p_precompile_task->offset + type->structure_type_info.members[i].data_member_location,
					metac_type_byte_size(type->structure_type_info.members[i].type)) == NULL) {
				msg_stderr("create_and_add_precompile_task failed\n");
				return -EFAULT;
			}
		}
	} break;
	case DW_TAG_array_type:
	case DW_TAG_pointer_type: {
		metac_array_elements_count_cb_ptr_t array_elements_count_funtion_ptr;
		void *	array_elements_count_cb_context;
		struct _region_type * array_elements__region_type = NULL;
		metac_type_t * array_elements_type = NULL;

		const metac_type_specification_value_t * spec = metac_type_specification(
				p_precompile_context->precompiled_type->type, p_precompile_task->region_type_element->path_global);

		/*get type array consists of or pointer points to*/
		switch(p_precompile_task->actual_type->id) {
			case DW_TAG_pointer_type:
				array_elements_type = p_precompile_task->actual_type->pointer_type_info.type;
			break;
			case DW_TAG_array_type:
				array_elements_type = p_precompile_task->actual_type->array_type_info.type;
			break;
		}
		if (array_elements_type == NULL /* void* */ ||
			array_elements_type->id == DW_TAG_subprogram){
			msg_stddbg("Warning: The type at %s can't be parsed futher - skipping its children\n",
					p_precompile_task->region_type_element->path_global);
			return 0;
		}

		if (spec == NULL || spec->array_elements_count_funtion_ptr == NULL) {
			msg_stddbg("Warning: Can't get array/pointer spec at %s - skipping its children\n",
					p_precompile_task->region_type_element->path_global);
			return 0;
		}
		array_elements__region_type = find_or_create_region_type(p_precompile_context, array_elements_type);
		if (array_elements__region_type == NULL) {
			msg_stderr("ERROR: cant create region_type - exiting\n");
			return -EFAULT;
		}

		msg_stddbg("p_precompile_task %p array_elements__region_type %p\n", p_precompile_task, array_elements__region_type);
		update_region_type_element_array_params(p_precompile_task->region_type_element,
				array_elements_count_funtion_ptr, array_elements_count_cb_context, array_elements__region_type->p_region_type);

		if (create_and_add_precompile_task(
				p_breadthfirst_engine,
				p_precompile_task,
				array_elements__region_type,
				array_elements_type,
				_parse_type_task,
				_parse_type_task_destroy,
				NULL, 0,
				"", "<ptr>",
				0,
				metac_type_byte_size(array_elements_type)) == NULL) {
			msg_stderr("create_and_add_precompile_task failed\n");
			return -EFAULT;
		}
	}break;
	}

	return 0;
}

/*****************************************************************************/
static int _phase2_calc_elements_per_type(
	struct breadthfirst_engine * p_breadthfirst_engine,
	struct breadthfirst_engine_task * p_breadthfirst_engine_task) {
	struct precompile_context * p_precompile_context = (struct precompile_context *)p_breadthfirst_engine->private_data;
	struct precompile_task * p_precompile_task = cds_list_entry(p_breadthfirst_engine_task, struct precompile_task, task);

	switch (p_precompile_task->actual_type->id) {
		case DW_TAG_base_type:
			++p_precompile_task->_region_type->p_region_type->base_type_elements_count;
			break;
		case DW_TAG_enumeration_type:
			++p_precompile_task->_region_type->p_region_type->enum_type_elements_count;
			break;
		case DW_TAG_pointer_type:
			++p_precompile_task->_region_type->p_region_type->pointer_type_elements_count;
			break;
		case DW_TAG_array_type:
			++p_precompile_task->_region_type->p_region_type->array_type_elements_count;
			break;
		default:
			++p_precompile_task->_region_type->p_region_type->hierarchy_elements_count;
	}
	++p_precompile_task->_region_type->p_region_type->elements_count;
	return 0;
}

static int _phase2_put_elements_per_type(
	struct breadthfirst_engine * p_breadthfirst_engine,
	struct breadthfirst_engine_task * p_breadthfirst_engine_task) {
	struct precompile_context * p_precompile_context = (struct precompile_context *)p_breadthfirst_engine->private_data;
	struct precompile_task * p_precompile_task = cds_list_entry(p_breadthfirst_engine_task, struct precompile_task, task);

	switch (p_precompile_task->actual_type->id) {
		case DW_TAG_base_type:
			p_precompile_task->_region_type->p_region_type->base_type_element[p_precompile_task->_region_type->p_region_type->base_type_elements_count] = p_precompile_task->region_type_element;
			++p_precompile_task->_region_type->p_region_type->base_type_elements_count;
			break;
		case DW_TAG_enumeration_type:
			p_precompile_task->_region_type->p_region_type->enum_type_element[p_precompile_task->_region_type->p_region_type->enum_type_elements_count] = p_precompile_task->region_type_element;
			++p_precompile_task->_region_type->p_region_type->enum_type_elements_count;
			break;
		case DW_TAG_pointer_type:
			p_precompile_task->_region_type->p_region_type->pointer_type_element[p_precompile_task->_region_type->p_region_type->pointer_type_elements_count] = p_precompile_task->region_type_element;
			++p_precompile_task->_region_type->p_region_type->pointer_type_elements_count;
			break;
		case DW_TAG_array_type:
			p_precompile_task->_region_type->p_region_type->array_type_element[p_precompile_task->_region_type->p_region_type->array_type_elements_count] = p_precompile_task->region_type_element;
			++p_precompile_task->_region_type->p_region_type->array_type_elements_count;
			break;
		default:
			p_precompile_task->_region_type->p_region_type->hierarchy_element[p_precompile_task->_region_type->p_region_type->hierarchy_elements_count] = p_precompile_task->region_type_element;
			++p_precompile_task->_region_type->p_region_type->hierarchy_elements_count;
	}
	p_precompile_task->_region_type->p_region_type->element[p_precompile_task->_region_type->p_region_type->elements_count] = p_precompile_task->region_type_element;
	++p_precompile_task->_region_type->p_region_type->elements_count;
	return 0;
}

static int _phase2(
		struct breadthfirst_engine* p_breadthfirst_engine,
		struct precompile_context *p_precompile_context){
	int i = 0;
	struct _region_type * _region_type;
	struct _discriminator * _discriminator;

	/* allocate memory for region_type array*/
	p_precompile_context->precompiled_type->region_type = calloc(p_precompile_context->precompiled_type->region_types_count, sizeof(*p_precompile_context->precompiled_type->region_type));
	if (p_precompile_context->precompiled_type->region_type == NULL) {
		msg_stderr("Can't allocate memory for region_type\n");
		return -ENOMEM;
	}
	/* collect region_types and their discriminators */
	cds_list_for_each_entry(_region_type, &p_precompile_context->region_type_list, list) {
		int j = 0;

		p_precompile_context->precompiled_type->region_type[i] = _region_type->p_region_type;

		p_precompile_context->precompiled_type->region_type[i]->discriminator =
				calloc(p_precompile_context->precompiled_type->region_type[i]->discriminators_count,
						sizeof(*p_precompile_context->precompiled_type->region_type[i]->discriminator));
		if (p_precompile_context->precompiled_type->region_type[i]->discriminator == NULL) {
			msg_stderr("Can't allocate memory for region_type->discriminator\n");
			return -ENOMEM;
		}
		cds_list_for_each_entry(_discriminator, &_region_type->discriminator_list, list) {
			_discriminator->p_discriminator->id = j;
			p_precompile_context->precompiled_type->region_type[i]->discriminator[j] = _discriminator->p_discriminator;
			j++;
		}
		i++;
	}

	run_breadthfirst_engine(p_breadthfirst_engine, _phase2_calc_elements_per_type);

	for (i = 0; i < p_precompile_context->precompiled_type->region_types_count; i++) {
		p_precompile_context->precompiled_type->region_type[i]->element = calloc(
				p_precompile_context->precompiled_type->region_type[i]->elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_type[i]->element));
		p_precompile_context->precompiled_type->region_type[i]->base_type_element = calloc(
				p_precompile_context->precompiled_type->region_type[i]->base_type_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_type[i]->base_type_element));
		p_precompile_context->precompiled_type->region_type[i]->enum_type_element = calloc(
				p_precompile_context->precompiled_type->region_type[i]->enum_type_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_type[i]->enum_type_element));
		p_precompile_context->precompiled_type->region_type[i]->array_type_element = calloc(
				p_precompile_context->precompiled_type->region_type[i]->array_type_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_type[i]->array_type_element));
		p_precompile_context->precompiled_type->region_type[i]->pointer_type_element = calloc(
				p_precompile_context->precompiled_type->region_type[i]->pointer_type_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_type[i]->pointer_type_element));
		p_precompile_context->precompiled_type->region_type[i]->hierarchy_element = calloc(
				p_precompile_context->precompiled_type->region_type[i]->hierarchy_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_type[i]->hierarchy_element));
		if (	(p_precompile_context->precompiled_type->region_type[i]->elements_count > 0 && p_precompile_context->precompiled_type->region_type[i]->element == NULL)
			||	(p_precompile_context->precompiled_type->region_type[i]->base_type_elements_count > 0 && p_precompile_context->precompiled_type->region_type[i]->base_type_element == NULL)
			||	(p_precompile_context->precompiled_type->region_type[i]->enum_type_elements_count > 0 && p_precompile_context->precompiled_type->region_type[i]->enum_type_element == NULL)
			||	(p_precompile_context->precompiled_type->region_type[i]->array_type_elements_count > 0 && p_precompile_context->precompiled_type->region_type[i]->array_type_element == NULL)
			||	(p_precompile_context->precompiled_type->region_type[i]->pointer_type_elements_count > 0 && p_precompile_context->precompiled_type->region_type[i]->pointer_type_element == NULL)
			||	(p_precompile_context->precompiled_type->region_type[i]->hierarchy_elements_count > 0 && p_precompile_context->precompiled_type->region_type[i]->hierarchy_element == NULL)
			) {
			msg_stderr("Can't allocate memory for pointers of elements\n");
			return -ENOMEM;
		}
		p_precompile_context->precompiled_type->region_type[i]->elements_count = 0;
		p_precompile_context->precompiled_type->region_type[i]->base_type_elements_count = 0;
		p_precompile_context->precompiled_type->region_type[i]->enum_type_elements_count = 0;
		p_precompile_context->precompiled_type->region_type[i]->array_type_elements_count = 0;
		p_precompile_context->precompiled_type->region_type[i]->pointer_type_elements_count = 0;
		p_precompile_context->precompiled_type->region_type[i]->hierarchy_elements_count = 0;
	}

	run_breadthfirst_engine(p_breadthfirst_engine, _phase2_put_elements_per_type);
	return 0;
}
/*****************************************************************************/
void cleanup_precompile_context(struct precompile_context *p_precompile_context) {
	struct _region_type * _region_type, * __region_type;
	struct _discriminator * _discriminator, * __discriminator;

	cds_list_for_each_entry_safe(_region_type, __region_type, &p_precompile_context->region_type_list, list) {
		cds_list_for_each_entry_safe(_discriminator, __discriminator, &_region_type->discriminator_list, list) {
			cds_list_del(&_discriminator->list);
			free(_discriminator);
		}
		cds_list_del(&_region_type->list);
		free(_region_type);
	}
}
/*****************************************************************************/
metac_precompiled_type_t * metac_precompile_type(struct metac_type *type) {
	struct breadthfirst_engine* p_breadthfirst_engine;
	struct precompile_context context;

	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return NULL;
	}

	context.precompiled_type = create_precompiled_type(type);
	if (context.precompiled_type == NULL) {
		msg_stderr("create_precompiled_type failed\n");
		return NULL;
	}
	CDS_INIT_LIST_HEAD(&context.region_type_list);

	/*use breadthfirst_engine*/
	p_breadthfirst_engine = create_breadthfirst_engine();
	if (p_breadthfirst_engine == NULL){
		msg_stderr("create_breadthfirst_engine failed\n");
		delete_precompiled_type(&context.precompiled_type);
		return NULL;
	}
	p_breadthfirst_engine->private_data = &context;

	if (create_and_add_precompile_task(p_breadthfirst_engine, NULL, find_or_create_region_type(&context, type),
			type,
			_parse_type_task,
			_parse_type_task_destroy,
			NULL, 0, "", "<ptr>", 0, metac_type_byte_size(type)) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");
		/*TBD: test if it's ok - not sure, because some of objects are not linked at that time*/
		cleanup_precompile_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_precompiled_type(&context.precompiled_type);
		return NULL;
	}
	if (run_breadthfirst_engine(p_breadthfirst_engine, NULL) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");
		cleanup_precompile_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_precompiled_type(&context.precompiled_type);
		return NULL;
	}

	if (_phase2(p_breadthfirst_engine, &context) != 0) {
		msg_stderr("Phase 2 returned error\n");
		cleanup_precompile_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		delete_precompiled_type(&context.precompiled_type);
		return NULL;
	}

	cleanup_precompile_context(&context);
	delete_breadthfirst_engine(&p_breadthfirst_engine);
	return context.precompiled_type;
}

void metac_dump_precompiled_type(metac_precompiled_type_t * precompiled_type) {
	/*TBD:*/
}

void metac_free_precompiled_type(metac_precompiled_type_t ** pp_precompiled_type) {
	delete_precompiled_type(pp_precompiled_type);
}

#if 0
/*****************************************************************************/
//static int _init_path(char**p_path, char *path, char *name) {
//	size_t path_len = path?(strlen(path)+1):0;
//	size_t name_len = name?(strlen(name)):0;
//
//	(*p_path) = calloc(1, path_len + name_len + 1 /*"\0"*/);
//	if ((*p_path) == NULL) {
//		msg_stderr("no memory\n");
//		return -1;
//	}
//
//	if (path) {
//		strcpy((*p_path), path);
//		if (name_len > 0)
//			strcpy(&(*p_path)[path_len - 1], ".");
//	}
//	strcpy(&(*p_path)[path_len], name);
//	return 0;
//}
//
//static struct _step * create__step(
//		int memobj_id,
//		char *global_path, char *global_generated_name, char *path, char *generated_name, char *name,
//		struct metac_type *type,
//		metac_data_member_location_t offset,
//		metac_byte_size_t byte_size,
//		struct _step * parent) {
//	struct _step *_step;
//
//	if (name == NULL || type == NULL) {
//		msg_stderr("invalid argument\n");
//		return NULL;
//	}
//
//	/* allocate object */
//	_step = calloc(1, sizeof(*_step));
//	if (_step == NULL) {
//		msg_stderr("no memory\n");
//		return NULL;
//	}
//	_step->p_step = calloc(1, sizeof(*(_step->p_step)));
//	if (_step->p_step == NULL) {
//		msg_stderr("no memory\n");
//		free(_step);
//		return NULL;
//	}
//
//	/* save type without typedefs*/
//	_step->p_step->type = metac_type_typedef_skip(type);
//
//	/* generate and save path */
//	if (_init_path(&_step->p_step->path, path, generated_name) != 0) {
//		free(_step->p_step);
//		free(_step);
//		return NULL;
//	}
//	if (_init_path(&_step->p_step->global_path, global_path, global_generated_name) != 0) {
//		free(_step->p_step->path);
//		free(_step->p_step);
//		free(_step);
//		return NULL;
//	}
//	_step->p_step->name = name!=NULL?strdup(name):NULL;
//	if (_step->p_step->name == NULL) {
//		free(_step->p_step->global_path);
//		free(_step->p_step->path);
//		free(_step->p_step);
//		free(_step);
//	}
//
//	/*save offset/byte_size*/
//	_step->p_step->offset = offset;
//	_step->p_step->byte_size = byte_size;
//
//	_step->memobj_id = memobj_id;
//
//	if (parent != NULL) {
//		_step->p_step->check.p_condition = parent->p_step->check.p_condition;
//		_step->p_step->check.expected_value = parent->p_step->check.expected_value;
//	}
//
//	/*init defaults*/
//	_step->p_step->memobj_idx = -1;
//
//	return _step;
//}
//
//static void delete_step(struct step * p_step) {
//	if (p_step) {
//		if (p_step->path) {
//			free(p_step->path);
//			p_step->path = NULL;
//		}
//		if (p_step->global_path) {
//			free(p_step->global_path);
//			p_step->global_path = NULL;
//		}
//		if (p_step->name) {
//			free(p_step->name);
//			p_step->name = NULL;
//		}
//		free(p_step);
//	}
//}
//
//static void delete__step(struct _step * _step) {
//	if (_step) {
//		delete_step(_step->p_step);
//		_step->p_step = NULL;
//		free(_step);
//	}
//}
//
//static struct _condition * create__condition(
//		int memobj_id,
//		metac_discriminator_funtion_ptr_t condition_fn_ptr,
//		void * context,
//		struct _step * parent_step) {
//	struct _condition *_condition;
//
//	/* allocate object */
//	_condition = calloc(1, sizeof(*_condition));
//	if (_condition == NULL) {
//		msg_stderr("no memory\n");
//		return NULL;
//	}
//	_condition->p_condition = calloc(1, sizeof(*(_condition->p_condition)));
//	if (_condition->p_condition == NULL) {
//		msg_stderr("no memory\n");
//		free(_condition);
//		return NULL;
//	}
//
//	_condition->memobj_id = memobj_id;
//	_condition->p_condition->condition_fn_ptr = condition_fn_ptr;
//	_condition->p_condition->context = context;
//
//	/*copy check from parent*/
//	_condition->p_condition->check.p_condition = parent_step->p_step->check.p_condition;
//	_condition->p_condition->check.expected_value = parent_step->p_step->check.expected_value;
//
//	return _condition;
//}
//
//static void delete_condition(struct condition *p_condition) {
//	if (p_condition) {
//		free(p_condition);
//	}
//}
//
//static void delete__condition(struct _condition *_condition) {
//	if (_condition) {
//		delete_condition(_condition->p_condition);
//		free(_condition);
//	}
//}

static struct _memobj * create__memobj(
		int memobj_id,
		metac_type_t * type) {
	struct _memobj *_memobj;

	/* allocate object */
	_memobj = calloc(1, sizeof(*_memobj));
	if (_memobj == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	_memobj->p_memobj = calloc(1, sizeof(*(_memobj->p_memobj)));
	if (_memobj->p_memobj == NULL) {
		msg_stderr("no memory\n");
		free(_memobj);
		return NULL;
	}

	_memobj->memobj_id = memobj_id;
	_memobj->p_memobj->type = type;
	return _memobj;
}

static void delete_memobj(struct memobj *p_memobj) {
	if (p_memobj) {
		free(p_memobj);
	}
}

static void delete__memobj(struct _memobj *_memobj) {
	if (_memobj) {
		delete_memobj(_memobj->p_memobj);
		free(_memobj);
	}
}

static struct _memobj * _find_memobj_by_type(
		struct cds_list_head * memobjs_list,
		metac_type_t * type) {
	struct _memobj * _memobj;

	cds_list_for_each_entry(_memobj, memobjs_list, list) {
		if (_memobj->p_memobj->type == type)
			return _memobj;
	}
	return NULL;
}

//static struct _memobj * _find_memobj_by_id(
//		struct cds_list_head * memobjs_list,
//		int memobj_id) {
//	struct _memobj * _memobj;
//
//	cds_list_for_each_entry(_memobj, memobjs_list, list) {
//		if (_memobj->memobj_id == memobj_id)
//			return _memobj;
//	}
//	return NULL;
//}

static int _phase1(
		metac_precompiled_type_t * precompiled_type,
		struct cds_list_head * memobjs_list,
		struct cds_list_head * steps_list,
		struct cds_list_head * conditions_list) { /*go width, validate and generate objects */
	struct cds_list_head *pos;
	struct _memobj * _memobj;
	struct _step * _next_step;

	/* put first element int list*/
	_memobj = create__memobj(precompiled_type->memobjs_count++, /*metac_type_typedef_skip(*/precompiled_type->type/*)*/);
	if (_memobj == NULL)
		return -(ENOMEM);
	cds_list_add_tail(&_memobj->list, memobjs_list);
	_next_step =  create__step(
			_memobj->memobj_id,
			NULL, "", NULL, "", "", /*path*/
			_memobj->p_memobj->type,
			0, metac_type_byte_size(_memobj->p_memobj->type), /* offset, size */
			NULL);
	if (_next_step == NULL)
		return -(ENOMEM);
	cds_list_add_tail(&_next_step->list, steps_list);

	/* go width */
	cds_list_for_each(pos, steps_list) {
		int is_anon;
		struct _step * current_step = cds_list_entry(pos, struct _step, list);
		struct metac_type *type = current_step->p_step->type;

		/*TODO: check spec on global level - e.g. that will allow to make Stop for some type*/

		switch(type->id) {
		case DW_TAG_base_type:
		case DW_TAG_enumeration_type:
			/* leaf types - nothing to do*/
			break;
		case DW_TAG_pointer_type: {
			metac_type_t * ptr_to_type = type->pointer_type_info.type != NULL?/*metac_type_typedef_skip(*/type->pointer_type_info.type/*)*/:NULL;
			const metac_type_specification_value_t * spec = metac_type_specification(precompiled_type->type, current_step->p_step->global_path);

			/*TBD: check the local type for specifications*/
//			if (spec == NULL) {
//				/*type*/
//				_memobj = _find_memobj_by_id(memobjs_list, current_step->memobj_id);
//				assert(_memobj != NULL);
//				spec = metac_type_specification(_memobj->p_memobj->type, current_step->p_step->path /*per memobj path*/);
//			}

			if (spec == NULL ||
				ptr_to_type == NULL ||
				spec->array_mode == amStop ||
				(spec->array_mode == amExtendAsArrayWithLen && spec->array_elements_count_funtion_ptr == NULL)) {
				msg_stddbg("Warning: Pointer %s doesn't have a pointer-type specification - skipping its children\n", current_step->p_step->global_path);
				/*TODO: by default if spec isn't specified - use amStop for void*, amExtendAsArrayWithNullEnd for char* and amExtendAsOneObject for the rest*/
				/*TODO: mark to skip the step */
				current_step->p_step->array_mode = amStop;
				continue;
			}

//			msg_stddbg("Pointer %s am%d\n", current_step->p_step->global_path, spec->array_mode);
			/*set array mode*/
			current_step->p_step->array_mode = spec->array_mode;
			current_step->p_step->array_elements_count_funtion_ptr = spec->array_elements_count_funtion_ptr;
			current_step->p_step->context = spec->specification_context;

			/*find or allocate new mem object id */
			_memobj = _find_memobj_by_type(memobjs_list, ptr_to_type);
			if (_memobj != NULL) {
				current_step->p_step->memobj_idx = _memobj->memobj_id;
				break;
			}

			_memobj = create__memobj(precompiled_type->memobjs_count++, ptr_to_type);
			if (_memobj == NULL)
				return -(ENOMEM);
			cds_list_add_tail(&_memobj->list, memobjs_list);
			current_step->p_step->memobj_idx = _memobj->memobj_id;
			_next_step = create__step(
					_memobj->memobj_id,
					current_step->p_step->global_path, "<ptr>", NULL, "", "",/*path*/
					_memobj->p_memobj->type,
					0, metac_type_byte_size(_memobj->p_memobj->type), /*offset/byte_size*/
					current_step);
			if (_next_step == NULL)
				return -(ENOMEM);
			cds_list_add_tail(&_next_step->list, steps_list);

			/*reset conditions*/
			_next_step->p_step->check.p_condition = NULL;
			_next_step->p_step->check.expected_value = 0;
		} break;
		case DW_TAG_array_type: {
			int i;
			metac_byte_size_t element_size;
			metac_type_t * element_type = type->array_type_info.type != NULL?/*metac_type_typedef_skip(*/type->array_type_info.type/*)*/:NULL;
			const metac_type_specification_value_t * spec = metac_type_specification(precompiled_type->type, current_step->p_step->global_path);

			if (type->array_type_info.is_flexible == 1) {
				msg_stddbg("Warning: Array %s is flexible. Isn't supported so far\n", current_step->p_step->global_path);
				break;
			}

			if (spec == NULL ||
				element_type == NULL ||
				spec->array_mode == amStop ||
				(spec->array_mode == amExtendAsArrayWithLen && spec->array_elements_count_funtion_ptr == NULL)) {
				msg_stddbg("Warning: Array %s doesn't have a array specification - skipping it\n", current_step->p_step->global_path);
				current_step->p_step->array_mode = amStop;
				continue;
			}

			/*set array mode*/
			current_step->p_step->array_mode = spec->array_mode;
			current_step->p_step->array_elements_count_funtion_ptr = spec->array_elements_count_funtion_ptr;
			current_step->p_step->context = spec->specification_context;

			/*find or allocate new mem object id */
			_memobj = _find_memobj_by_type(memobjs_list, type);
			if (_memobj != NULL) {
				current_step->p_step->memobj_idx = _memobj->memobj_id;
				break;
			}

			_memobj = create__memobj(precompiled_type->memobjs_count++, type);
			if (_memobj == NULL)
				return -(ENOMEM);
			cds_list_add_tail(&_memobj->list, memobjs_list);
			current_step->p_step->memobj_idx = _memobj->memobj_id;
			_next_step = create__step(
					_memobj->memobj_id,
					current_step->p_step->global_path, "<array>", NULL, "", "",/*path*/
					_memobj->p_memobj->type,
					0, metac_type_byte_size(_memobj->p_memobj->type), /*offset/byte_size*/
					current_step);
			if (_next_step == NULL)
				return -(ENOMEM);
			cds_list_add_tail(&_next_step->list, steps_list);

			/*reset conditions*/
			_next_step->p_step->check.p_condition = NULL;
			_next_step->p_step->check.expected_value = 0;
		} break;
		case DW_TAG_structure_type: {
			int anon_members_count = 0;
			metac_num_t i;
			for (i = 0; i < type->structure_type_info.members_count; i++) {
				char anon_name[15];
				/*todo: check if the next type is flexible array and it's not the last element*/

				is_anon = 0;
				if (strcmp(type->structure_type_info.members[i].name, "") == 0) {
					is_anon = 1;
					snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
				}

				_next_step = create__step(
						current_step->memobj_id,
						current_step->p_step->global_path,
						is_anon?anon_name:type->structure_type_info.members[i].name,
						current_step->p_step->path,
						is_anon?/*anon_name*/"":type->structure_type_info.members[i].name,
						type->structure_type_info.members[i].name, /*path*/
						type->structure_type_info.members[i].type,
						current_step->p_step->offset + type->structure_type_info.members[i].data_member_location,
						metac_type_byte_size(type->structure_type_info.members[i].type), /*offset/byte_size*/
						current_step);
				if (_next_step == NULL)
					return -(ENOMEM);
				cds_list_add_tail(&_next_step->list, steps_list);

				_next_step->p_step->is_anon = is_anon;
			}
		} break;
		case DW_TAG_union_type: {
			struct _condition * _condition;
			int anon_members_count = 0;
			metac_num_t i;
			/* try to find discriminator ptr */
			const metac_type_specification_value_t * spec = metac_type_specification(precompiled_type->type, current_step->p_step->global_path);
			if (spec == NULL || spec->discriminator_funtion_ptr == NULL) {
				msg_stddbg("Warning: Union %s doesn't have a union-type specification - skipping its children\n", current_step->p_step->global_path);
				/*TODO: mark to skip the step */
				continue;
			}

			/*allocate struct to keep conditions point to fn_ptr*/
			_condition = create__condition(current_step->memobj_id,
					spec->discriminator_funtion_ptr,
					spec->specification_context,
					current_step);
			if (_condition == NULL)
				return -(ENOMEM);
			cds_list_add_tail(&_condition->list, conditions_list);

			for (i = 0; i < type->union_type_info.members_count; i++) {
				char anon_name[15];

				is_anon = 0;
				if (strcmp(type->union_type_info.members[i].name, "") == 0) {
					is_anon = 1;
					snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
				}
				_next_step = create__step(
						current_step->memobj_id,
						current_step->p_step->global_path,
						is_anon?anon_name:type->union_type_info.members[i].name,
						current_step->p_step->path,
						is_anon?""/*anon_name*/:type->union_type_info.members[i].name,
						type->union_type_info.members[i].name,/*path*/
						type->union_type_info.members[i].type,
						current_step->p_step->offset + type->union_type_info.members[i].data_member_location,
						metac_type_byte_size(type->union_type_info.members[i].type), /*offset/byte_size*/
						current_step);
				if (_next_step == NULL)
					return -(ENOMEM);
				cds_list_add_tail(&_next_step->list, steps_list);

				_next_step->p_step->is_anon = is_anon;
				/* override check for this step using new condition */
				_next_step->p_step->check.p_condition = _condition->p_condition;
				_next_step->p_step->check.expected_value = i;
			}
		} break;
		case DW_TAG_subprogram: {
			msg_stddbg("ERROR: Can't compile subprogram types\n");
			return -1;
		} break;
		}
	}
	return 0;
}

static void _free_phase1_lists(
		struct cds_list_head * memobjs_list,
		struct cds_list_head * steps_list,
		struct cds_list_head * conditions_list) {
	struct _memobj * _memobj, * __memobj;
	struct _condition * _condition, * __condition;
	struct _step * _step, * __step;

	cds_list_for_each_entry_safe(_memobj, __memobj, memobjs_list, list) {
		cds_list_del(&_memobj->list);
		delete__memobj(_memobj);
	}

	cds_list_for_each_entry_safe(_step, __step, steps_list, list) {
		cds_list_del(&_step->list);
		delete__step(_step);
	}

	cds_list_for_each_entry_safe(_condition, __condition, conditions_list, list) {
		cds_list_del(&_condition->list);
		delete__condition(_condition);
	}
}

static int _phase2(
		metac_precompiled_type_t * precompiled_type,
		struct cds_list_head * memobjs_list,
		struct cds_list_head * steps_list,
		struct cds_list_head * conditions_list){
	int i = 0;
	struct _memobj * _memobj, * __memobj;
	struct _condition * _condition, * __condition;
	struct _step * _step, * __step;

	/* allocate memory for memobjs*/
	precompiled_type->memobj = calloc(precompiled_type->memobjs_count, sizeof(*precompiled_type->memobj));
	if (precompiled_type->memobj == NULL) {
		msg_stderr("Can't allocate memory\n");
		return -1;
	}

	cds_list_for_each_entry(_memobj, memobjs_list, list) {
		assert(i == _memobj->memobj_id);
		precompiled_type->memobj[i++] = _memobj->p_memobj;
	}

	cds_list_for_each_entry(_step, steps_list, list) {
		i = _step->memobj_id;
		assert(i < precompiled_type->memobjs_count);

		switch (_step->p_step->type->id) {
			case DW_TAG_base_type:
				++precompiled_type->memobj[i]->base_type_steps_count;
				break;
			case DW_TAG_enumeration_type:
				++precompiled_type->memobj[i]->enum_type_steps_count;
				break;
			case DW_TAG_pointer_type:
				++precompiled_type->memobj[i]->pointer_type_steps_count;
				break;
			case DW_TAG_array_type:
				++precompiled_type->memobj[i]->array_type_steps_count;
				break;
		}
		++precompiled_type->memobj[i]->steps_count;
	}
	/*get destibution of conditions over memobjs*/
	cds_list_for_each_entry(_condition, conditions_list, list) {
		i = _condition->memobj_id;
		++precompiled_type->memobj[i]->conditions_count;
	}

	for (i = 0; i < precompiled_type->memobjs_count; i++) {
		/* alloc memory for conditions */
		if (precompiled_type->memobj[i]->conditions_count > 0){
			precompiled_type->memobj[i]->condition = calloc(precompiled_type->memobj[i]->conditions_count, sizeof(*precompiled_type->memobj[i]->condition));
			if (precompiled_type->memobj[i]->condition == NULL) {
				return -1;
			}
		}
		/* alloc memory for steps */
		if (precompiled_type->memobj[i]->steps_count > 0) {
			precompiled_type->memobj[i]->step = calloc(precompiled_type->memobj[i]->steps_count, sizeof(*precompiled_type->memobj[i]->step));
			if (precompiled_type->memobj[i]->step == NULL) {
				return -1;
			}

#if 0
			msg_stddbg("all %d p %d b %d e %d a d%\n",
					precompiled_type->memobj[i].steps_count,
					precompiled_type->memobj[i].pointer_type_steps_count,
					precompiled_type->memobj[i].base_type_steps_count,
					precompiled_type->memobj[i].enum_type_steps_count,
					precompiled_type->memobj[i].array_type_steps_count);
#endif
			/*put leafs to the end of array*/
			precompiled_type->memobj[i]->array_type_idx = precompiled_type->memobj[i]->steps_count -
					precompiled_type->memobj[i]->array_type_steps_count -
					precompiled_type->memobj[i]->pointer_type_steps_count -
					precompiled_type->memobj[i]->base_type_steps_count -
					precompiled_type->memobj[i]->enum_type_steps_count;
			precompiled_type->memobj[i]->pointer_type_idx = precompiled_type->memobj[i]->array_type_idx +
					precompiled_type->memobj[i]->array_type_steps_count;
			precompiled_type->memobj[i]->base_type_idx = precompiled_type->memobj[i]->pointer_type_idx +
					precompiled_type->memobj[i]->pointer_type_steps_count;
			precompiled_type->memobj[i]->enum_type_idx = precompiled_type->memobj[i]->base_type_idx +
					precompiled_type->memobj[i]->base_type_steps_count;
#if 0
			msg_stddbg("a %d p %d b %d e %d\n",
					precompiled_type->memobj[i]->array_type_idx,
					precompiled_type->memobj[i]->pointer_type_idx,
					precompiled_type->memobj[i]->base_type_idx,
					precompiled_type->memobj[i]->enum_type_idx);
#endif
			assert(precompiled_type->memobj[i]->enum_type_idx + precompiled_type->memobj[i]->enum_type_steps_count ==
					precompiled_type->memobj[i]->steps_count);
		}

		/* reset counters - will use them to fill in arrays */
		precompiled_type->memobj[i]->array_type_steps_count = 0;
		precompiled_type->memobj[i]->pointer_type_steps_count = 0;
		precompiled_type->memobj[i]->base_type_steps_count = 0;
		precompiled_type->memobj[i]->enum_type_steps_count = 0;
		precompiled_type->memobj[i]->steps_count = 0;
		precompiled_type->memobj[i]->conditions_count = 0;
	}

	cds_list_for_each_entry(_step, steps_list, list) {
		int j;
		i = _step->memobj_id;
		switch (_step->p_step->type->id) {
			case DW_TAG_base_type:
				j = precompiled_type->memobj[i]->base_type_steps_count++;
				precompiled_type->memobj[i]->step[precompiled_type->memobj[i]->base_type_idx + j] = _step->p_step;
				break;
			case DW_TAG_enumeration_type:
				j = precompiled_type->memobj[i]->enum_type_steps_count++;
				precompiled_type->memobj[i]->step[precompiled_type->memobj[i]->enum_type_idx + j] = _step->p_step;
				break;
			case DW_TAG_pointer_type:
				j = precompiled_type->memobj[i]->pointer_type_steps_count++;
				precompiled_type->memobj[i]->step[precompiled_type->memobj[i]->pointer_type_idx + j] = _step->p_step;
				break;
			case DW_TAG_array_type:
				j = precompiled_type->memobj[i]->array_type_steps_count++;
				precompiled_type->memobj[i]->step[precompiled_type->memobj[i]->array_type_idx + j] = _step->p_step;
				break;
			default:
				j = precompiled_type->memobj[i]->steps_count++;
				precompiled_type->memobj[i]->step[j] = _step->p_step;
		}
	}
	cds_list_for_each_entry(_condition, conditions_list, list) {
		int j;
		i = _condition->memobj_id;
		j = precompiled_type->memobj[i]->conditions_count++;
		precompiled_type->memobj[i]->condition[j] = _condition->p_condition;
		precompiled_type->memobj[i]->condition[j]->value_index = j;
	}

	for (i = 0; i < precompiled_type->memobjs_count; i++) {
		int j;
		assert(precompiled_type->memobj[i]->steps_count == precompiled_type->memobj[i]->array_type_idx);
		assert(precompiled_type->memobj[i]->array_type_idx + precompiled_type->memobj[i]->array_type_steps_count == precompiled_type->memobj[i]->pointer_type_idx);
		assert(precompiled_type->memobj[i]->pointer_type_idx + precompiled_type->memobj[i]->pointer_type_steps_count == precompiled_type->memobj[i]->base_type_idx);
		assert(precompiled_type->memobj[i]->base_type_idx + precompiled_type->memobj[i]->base_type_steps_count == precompiled_type->memobj[i]->enum_type_idx);
		precompiled_type->memobj[i]->steps_count +=
				precompiled_type->memobj[i]->base_type_steps_count +
				precompiled_type->memobj[i]->enum_type_steps_count +
				precompiled_type->memobj[i]->pointer_type_steps_count +
				precompiled_type->memobj[i]->array_type_steps_count ;
		for (j = 0; j < precompiled_type->memobj[i]->steps_count; j++) {
			precompiled_type->memobj[i]->step[j]->value_index = j;
		}
	}

	/* partially free memory */
	cds_list_for_each_entry_safe(_memobj, __memobj, memobjs_list, list) {
		cds_list_del(&_memobj->list);
		free(_memobj);
	}

	cds_list_for_each_entry_safe(_step, __step, steps_list, list) {
		cds_list_del(&_step->list);
		free(_step);
	}

	cds_list_for_each_entry_safe(_condition, __condition, conditions_list, list) {
		cds_list_del(&_condition->list);
		free(_condition);
	}

	return 0;
}

metac_precompiled_type_t * metac_precompile_type(struct metac_type *type) {
	metac_precompiled_type_t * precompiled_type;
	struct cds_list_head memobjs_list;
	struct cds_list_head steps_list;
	struct cds_list_head conditions_list;

	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return NULL;
	}
	precompiled_type = calloc(1, sizeof(metac_precompiled_type_t));
	if (precompiled_type == NULL) {
		msg_stderr("Can't allocate memory for metac_precompiled_type_t\n");
		return NULL;
	}
	precompiled_type->type = type;

	CDS_INIT_LIST_HEAD(&memobjs_list);
	CDS_INIT_LIST_HEAD(&steps_list);
	CDS_INIT_LIST_HEAD(&conditions_list);

	if (_phase1(precompiled_type, &memobjs_list, &steps_list, &conditions_list) != 0) {
		msg_stderr("Phase 1 returned error\n");
		_free_phase1_lists(&memobjs_list, &steps_list, &conditions_list);
		free(precompiled_type);
		return NULL;
	}

#if 0
	do {
		struct _condition * _condition;
		struct _step * _step;
		printf("conditions:\n");
		cds_list_for_each_entry(_condition, &conditions_list, list) {
			msg_stddbg("%d fn: %p ctx: %s c(%p, %d)\n", _condition->memobj_id,
					_condition->p_condition->condition_fn_ptr, (char*)_condition->p_condition->context,
					_condition->p_condition->check.p_condition, _condition->p_condition->check.expected_value);
		}
		printf("steps:\n");
		cds_list_for_each_entry(_step, &steps_list, list) {
			msg_stddbg("%d/%d step %s %s [%d, %d) c(%p, %d)\n", _step->memobj_id, _step->p_step->memobj_idx,
					_step->p_step->type->name?_step->p_step->type->name:(_step->p_step->type->id == DW_TAG_pointer_type)?"ptr":"<no type name>",
					_step->p_step->path,
					_step->p_step->offset, _step->p_step->byte_size,
					_step->p_step->check.p_condition, _step->p_step->check.expected_value);
		}
	}while(0);
#endif

	if (_phase2(precompiled_type, &memobjs_list, &steps_list, &conditions_list) != 0) {
		msg_stderr("Phase 1 returned error\n");
		_free_phase1_lists(&memobjs_list, &steps_list, &conditions_list);
		free(precompiled_type);
		return NULL;
	}

	return precompiled_type;
}

void metac_dump_precompiled_type(metac_precompiled_type_t * precompiled_type) {
	if (precompiled_type == NULL ||
		precompiled_type->type == NULL)
		return;

	if (precompiled_type->memobj != NULL ) {
		int i;
		printf("--DUMP for %s--\n", precompiled_type->type->name?precompiled_type->type->name:"(nil)");
		for (i = 0; i < precompiled_type->memobjs_count; i++){
			int j;
			printf("Memobj %d\n", i);
			if (precompiled_type->memobj[i]->condition != NULL) {
				printf("Conditions:\n");
				for (j = 0; j < precompiled_type->memobj[i]->conditions_count; j++) {
					struct condition * condition = precompiled_type->memobj[i]->condition[j];
					printf("%d. addr %p fn: %p ctx: %s c(%p, %d)\n", j,
							condition,
							condition->condition_fn_ptr, (char*)condition->context,
							condition->check.p_condition, condition->check.expected_value);

				}
			}
			if (precompiled_type->memobj[i]->step != NULL) {
				printf("Steps: Pointer_Idx %d Base_Idx %d Enum_Idx %d\n",
						precompiled_type->memobj[i]->pointer_type_idx,
						precompiled_type->memobj[i]->base_type_idx,
						precompiled_type->memobj[i]->enum_type_idx);
				for (j = 0; j < precompiled_type->memobj[i]->steps_count; j++) {
					struct step * step = precompiled_type->memobj[i]->step[j];
					printf("%d. value_idx %d %s %s [%d, %d) c(%p, %d) name \"%s\" anon %d ptr %d am%d\n", j,
							step->value_index,
							step->type->name?step->type->name:(step->type->id == DW_TAG_pointer_type)?"ptr":"<no type name>",
							step->path,
							step->offset, step->byte_size,
							step->check.p_condition, step->check.expected_value,
							step->name!=NULL?step->name:"(nil)",
							step->is_anon,
							step->memobj_idx,
							step->array_mode);
				}
			}
		}
		printf("--END---\n");
	}
}

void metac_free_precompiled_type(metac_precompiled_type_t ** p_precompiled_type) {
	metac_precompiled_type_t * precompiled_type;
	if (p_precompiled_type == NULL) {
		msg_stderr("invalid argument value: precompiled_type\n");
		return;
	}
	if (*p_precompiled_type == NULL) {
		msg_stderr("already freed\n");
		return;
	}
	precompiled_type = *p_precompiled_type;
	if (precompiled_type->memobj != NULL ) {
		int i;
		for (i = 0; i < precompiled_type->memobjs_count; i++){
			int j;
			if (precompiled_type->memobj[i]->condition != NULL) {
				for (j = 0; j < precompiled_type->memobj[i]->conditions_count; j++) {
					delete_condition(precompiled_type->memobj[i]->condition[j]);
					precompiled_type->memobj[i]->condition[j] = NULL;
				}
				free(precompiled_type->memobj[i]->condition);
				precompiled_type->memobj[i]->condition = NULL;
			}
			if (precompiled_type->memobj[i]->step != NULL) {
				for (j = 0; j < precompiled_type->memobj[i]->steps_count; j++) {
					delete_step(precompiled_type->memobj[i]->step[j]);
					precompiled_type->memobj[i]->step[j] = NULL;
				}
				free(precompiled_type->memobj[i]->step);
				precompiled_type->memobj[i]->step = NULL;
			}
			free(precompiled_type->memobj[i]);
			precompiled_type->memobj[i] = NULL;
		}
		free(precompiled_type->memobj);
		precompiled_type->memobj = NULL;
	}

	free(*p_precompiled_type);
	*p_precompiled_type = NULL;
}
#endif
#if 0
/*****************************************************************************/
/**************work with real data *******************************************/
/*****************************************************************************/
struct memobj_work_data {
	struct cds_list_head list;

	struct memobj * memobj;
	void *	ptr;
	metac_byte_size_t size;

	struct {
		int init_flag;
		int value;
	}*condition_value;	/*memobj.conditions_count*/;
	struct wd_temp_data{
		metac_count_t count;
		int count_is_set;

		void * temp_data;
		/*todo: temp_data destructor */
	}* step_result;	/*memobj.steps_count*/;
};
struct precomiled_type_work_data {
	metac_precompiled_type_t * precompiled_type;	/*not really necessary, but it's just to check consistency */
	struct cds_list_head memobjwd_list;
};

typedef int (*step_visitor_t)(int write_operation, struct memobj_work_data* wd, struct step * step, void * context);

static struct memobj_work_data* create_wd(struct memobj * memobj, void *ptr, metac_byte_size_t size) {
	struct memobj_work_data * wd;

	wd = calloc(1, sizeof(*wd));
	if (wd == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	wd->memobj = memobj;

	wd->step_result = calloc(memobj->steps_count, sizeof(struct wd_temp_data));
	if (wd->step_result == NULL) {
		msg_stderr("no memory\n");
		return NULL;	/*TODO: fix these leaks!*/
	}

	wd->ptr = ptr;
	wd->size = size;
	return wd;
}

static void delete_wd(struct memobj_work_data* wd) {
	if (wd) {
		if (wd->condition_value){
			free(wd->condition_value);
			wd->condition_value = NULL;
		}
		if (wd->step_result){
			free(wd->step_result);
			wd->step_result = NULL;
		}
		free(wd);
	}
}

static int _visitor_check_condition(struct memobj_work_data* wd, struct step * step) {
	if (	step->check.p_condition != NULL && (
				wd->condition_value[step->check.p_condition->value_index].init_flag == 0 ||
				wd->condition_value[step->check.p_condition->value_index].value != step->check.expected_value
				)) {
		//msg_stddbg("Skipped %s\n", step->path);
		return -1;	/*condition doesn't match*/
	}
	//msg_stddbg("Do %s\n", step->global_path);
	return 0;
}

static int _init_conditions(struct precomiled_type_work_data * p_data,
		struct memobj_work_data* wd) {
	int i;
	struct memobj * memobj = wd->memobj;

	/* init conditions */
	if (memobj->conditions_count > 0) {
		wd->condition_value = calloc(memobj->conditions_count, sizeof(*wd->condition_value));
		if (wd->condition_value == NULL) {
			msg_stderr("no memory\n");
			return -(ENOMEM);	/*TODO: fix these leaks!*/
		}
		for (i = 0; i < memobj->conditions_count; i++) {
			if (memobj->condition[i]->check.p_condition &&
					wd->condition_value[memobj->condition[i]->check.p_condition->value_index].value != memobj->condition[i]->check.expected_value)
				continue;	/*don't need to initialize */
			if (memobj->condition[i]->condition_fn_ptr) {
				if (memobj->condition[i]->condition_fn_ptr(0, memobj->type,
						memobj->condition[i]->context, wd->ptr, &wd->condition_value[i].value) != 0) {
					msg_stderr("discriminator failed - skipping condition\n");
					//return -(EFAULT);
					//continue;
					wd->condition_value[i].value = -1;
				}
				wd->condition_value[i].init_flag = 1;
			}
		}
	}
	return 0;
}

#define _VISIT_(args...) \
	if (visitor)\
		visitor(args)

static int handle_wd_base_type(
		int write_operation,
		int forward_direction,
		struct precomiled_type_work_data * p_data,
		struct memobj_work_data* wd,
		step_visitor_t visitor, void * visitor_context) {
	int i;
	struct memobj * memobj = wd->memobj;
	for (i = memobj->base_type_idx; i < memobj->base_type_idx+memobj->base_type_steps_count; i++) {
		struct step * step = memobj->step[forward_direction?i:(memobj->base_type_idx+memobj->base_type_steps_count - i - 1)];
		if (_visitor_check_condition(wd, step) != 0)
			continue;
		/*visit base types */
		_VISIT_(write_operation, wd, step, visitor_context);
	}
	return 0;
}

static int handle_wd_enum_type(
		int write_operation,
		int forward_direction,
		struct precomiled_type_work_data * p_data,
		struct memobj_work_data* wd,
		step_visitor_t visitor, void * visitor_context) {
	int i;
	struct memobj * memobj = wd->memobj;
	for (i = memobj->enum_type_idx; i < memobj->enum_type_idx+memobj->enum_type_steps_count; i++) {
		struct step * step = memobj->step[forward_direction?i:(memobj->enum_type_idx+memobj->enum_type_steps_count - i - 1)];
		if (_visitor_check_condition(wd, step) != 0)
			continue;
		/*visit base types */
		_VISIT_(write_operation, wd, step, visitor_context);
	}
	return 0;
}

static int handle_wd_pointer_type(
		int write_operation,
		int forward_direction,
		struct precomiled_type_work_data * p_data,
		struct memobj_work_data* wd,
		step_visitor_t visitor, void * visitor_context) {
	int i;
	struct memobj * memobj = wd->memobj;
	for (i = memobj->pointer_type_idx; i < memobj->pointer_type_idx+memobj->pointer_type_steps_count; i++) {
		struct step * step = memobj->step[forward_direction?i:(memobj->pointer_type_idx+memobj->pointer_type_steps_count - i - 1)];
		int j;
		//int count_is_set = 1;
		wd->step_result[step->value_index].count_is_set = 1;
		wd->step_result[step->value_index].count = 0;

		if (_visitor_check_condition(wd, step) != 0)
			continue;

		/*TODO: store counter - and reuse it next time like conditions*/

		switch(step->array_mode) {
		case amStop: continue;	/*don't go here */
		case amExtendAsOneObject: {
			wd->step_result[step->value_index].count = 1;
		} break;
		case amExtendAsArrayWithLen:
			assert(step->array_elements_count_funtion_ptr != NULL);
			if (step->array_elements_count_funtion_ptr(write_operation,
					step->type, step->context, wd->ptr, 1,
					&wd->step_result[step->value_index].count) != 0) {
				msg_stderr("array_elements_count_funtion failed\n");
				continue;
			}
			break;
		case amExtendAsArrayWithNullEnd:
			wd->step_result[step->value_index].count_is_set = 0;
			break;
		}

		msg_stddbg("ptr: %s mode %d count %d\n", step->global_path, (int)step->array_mode, wd->step_result[step->value_index].count);

		/*and go to another memobj*/
		if (	wd->step_result[step->value_index].count_is_set != 0 &&
				wd->step_result[step->value_index].count > 0) {

			struct memobj_work_data* new_wd;
			int new_memobj_idx = step->memobj_idx;
			void *new_ptr = *((void**)(wd->ptr + step->offset));	/*read pointer value*/
			metac_byte_size_t new_byte_size = metac_type_byte_size(p_data->precompiled_type->memobj[step->memobj_idx]->type);

			if (new_ptr == NULL) {
				msg_stderr("met NULL\n");
				continue;
			}

			msg_stddbg("ptr: %s count %d\n", step->global_path, wd->step_result[step->value_index].count);
			for (j = 0; j < wd->step_result[step->value_index].count;  j++) { /* add array to queue */
				/*TODO: check if already had this addr in range [new_ptr + j * new_byte_size, new_byte_size) - loop*/

				new_wd = create_wd(p_data->precompiled_type->memobj[new_memobj_idx], new_ptr + j * new_byte_size, new_byte_size);
				if (new_wd == NULL) {
					msg_stderr("memobj_idx is too big\n");
					return -(ENOMEM);
				}
				cds_list_add_tail(&new_wd->list, &p_data->memobjwd_list);
			}
		}/*else*/

		/*visit base types */
		_VISIT_(write_operation, wd, step, visitor_context);
	}
	return 0;
}

static int handle_wd_array_type(
		int write_operation,
		int forward_direction,
		struct precomiled_type_work_data * p_data,
		struct memobj_work_data* wd,
		step_visitor_t visitor, void * visitor_context) {
	int i;
	struct memobj * memobj = wd->memobj;
	for (i = memobj->array_type_idx; i < memobj->array_type_idx+memobj->array_type_steps_count; i++) {
		struct step * step = memobj->step[forward_direction?i:(memobj->array_type_idx+memobj->array_type_steps_count - i - 1)];
		int j;
		//int count_is_set = 1;
		//metac_count_t count = 0;

		if (_visitor_check_condition(wd, step) != 0)
			continue;

		wd->step_result[step->value_index].count_is_set = 1;
		wd->step_result[step->value_index].count = 0;

		switch(step->array_mode) {
		case amStop: continue;	/*don't go here */
		case amExtendAsOneObject: {
			wd->step_result[step->value_index].count = 1;
		} break;
		case amExtendAsArrayWithLen:
			assert(step->array_elements_count_funtion_ptr != NULL);
			if (step->array_elements_count_funtion_ptr(write_operation,
					step->type, step->context, wd->ptr, step->type->array_type_info.subranges_count,
					&wd->step_result[step->value_index].count) != 0) {
				msg_stderr("array_elements_count_funtion failed\n");
				continue;
			}
			break;
		case amExtendAsArrayWithNullEnd:
			wd->step_result[step->value_index].count_is_set = 0;
			break;
		}

//		if (count_is_set != 0) {
//			for (j = 0; i < wd->step_result[step->value_index].count;  j++) {
//
//			}
//		}/*else*/

		/*visit base types */
		_VISIT_(write_operation, wd, step, visitor_context);
	}
	return 0;
}

static int handle_wd_struct_and_union_type(
		int write_operation,
		int forward_direction,
		struct precomiled_type_work_data * p_data,
		struct memobj_work_data* wd,
		step_visitor_t visitor, void * visitor_context) {
	int i;
	struct memobj * memobj = wd->memobj;
	int step_count =
			memobj->steps_count -
			memobj->base_type_steps_count -
			memobj->enum_type_steps_count -
			memobj->pointer_type_steps_count -
			memobj->array_type_steps_count;
	for (i = 0; i < step_count; i++) {
		struct step * step = memobj->step[forward_direction?i:(step_count - i - 1)];
		if (_visitor_check_condition(wd, step) != 0)
			continue;
		/*visit base types */
		_VISIT_(write_operation, wd, step, visitor_context);
	}
	return 0;
}

static int _read_visitor_pattern(metac_precompiled_type_t * precompiled_type, void *ptr, metac_byte_size_t size,
		step_visitor_t visitor1, void * visitor1_context, step_visitor_t visitor2, void * visitor2_context) {
	int i;
	struct memobj_work_data* wd, *_wd;
	struct precomiled_type_work_data data;

	data.precompiled_type = precompiled_type;
	CDS_INIT_LIST_HEAD(&data.memobjwd_list);

	/* init first wd */
	wd = create_wd(0, ptr, size);
	if (wd == NULL) {
		return -(ENOMEM);
	}
	cds_list_add_tail(&wd->list, &data.memobjwd_list);

	/* go width ip (pointers/arrays) */
	cds_list_for_each_entry(wd, &data.memobjwd_list, list) {
		/* init results*/
		struct memobj * memobj = wd->memobj;
		if (_init_conditions(&data, wd) != 0) {
			return -(EFAULT);
		}
		msg_stddbg("forward>>>>>>>>>>>\n");
		if (handle_wd_base_type(0, 1, &data, wd, visitor1, visitor1_context) != 0)
			return -(EFAULT);
		if (handle_wd_enum_type(0, 1, &data, wd, visitor1, visitor1_context) != 0)
			return -(EFAULT);
		if (handle_wd_pointer_type(0, 1, &data, wd, visitor1, visitor1_context) != 0)
			return -(EFAULT);
		if (handle_wd_array_type(0, 1, &data, wd, visitor1, visitor1_context) != 0)
			return -(EFAULT);
		if (handle_wd_struct_and_union_type(1, 0, &data, wd, visitor1, visitor1_context) != 0)
			return -(EFAULT);
		msg_stddbg("end forward>>>>>>>>>>>\n");
	}
	msg_stddbg("========================\n");
	/* go back (lower->upper) */
//	cds_list_for_each_entry_reverse(wd, &data.memobjwd_list, list) {
//		msg_stddbg("backward<<<<<<<<<<<<<\n");
//		if (handle_wd_base_type(0, 1, &data, wd, visitor2, visitor2_context) != 0)
//			return -(EFAULT);
//		if (handle_wd_enum_type(0, 1, &data, wd, visitor2, visitor2_context) != 0)
//			return -(EFAULT);
//		if (handle_wd_pointer_type(0, 1, &data, wd, visitor2, visitor2_context) != 0)
//			return -(EFAULT);
//		if (handle_wd_array_type(0, 1, &data, wd, visitor2, visitor2_context) != 0)
//			return -(EFAULT);
//		if (handle_wd_struct_and_union_type(1, 0, &data, wd, visitor2, visitor2_context) != 0)
//			return -(EFAULT);
//		msg_stddbg("end backward<<<<<<<<<<<<<\n");
//	}

	/*TODO: free everything */
	cds_list_for_each_entry_safe(wd, _wd, &data.memobjwd_list, list) {
		cds_list_del(&wd->list);
		delete_wd(wd);
	}

	return 0;
}

int metac_delete(metac_precompiled_type_t * precompiled_type, void *ptr, metac_byte_size_t size) {
	msg_stddbg("%p %p %d\n", precompiled_type, ptr, size);
	int i;
	struct memobj_work_data* wd, *_wd;
	struct precomiled_type_work_data data;

	if (precompiled_type == NULL) {
		msg_stderr("invalid argument value: precompiled_type\n");
		return -(EINVAL);
	}

	data.precompiled_type = precompiled_type;
	CDS_INIT_LIST_HEAD(&data.memobjwd_list);

	/* init first wd */
	wd = create_wd(data.precompiled_type->memobj[0], ptr, size);
	if (wd == NULL) {
		return -(ENOMEM);
	}
	cds_list_add_tail(&wd->list, &data.memobjwd_list);

	/* go width ip (pointers/arrays) */
	cds_list_for_each_entry(wd, &data.memobjwd_list, list) {
		/* init results*/
		if (_init_conditions(&data, wd) != 0) {
			return -(EFAULT);
		}

		if (handle_wd_pointer_type(0, 1, &data, wd, NULL, NULL) != 0)
			return -(EFAULT);
	}
	/* go in backwards direction */
	cds_list_for_each_entry_reverse(wd, &data.memobjwd_list, list) {
		struct memobj * memobj = wd->memobj;
		for (i = memobj->pointer_type_idx; i < memobj->pointer_type_idx+memobj->pointer_type_steps_count; i++) {
			struct step * step = memobj->step[i];
			if (_visitor_check_condition(wd, step) != 0)
				continue;
			void *new_src_ptr_val = *((void**)(wd->ptr + step->offset));
			if (	wd->step_result[step->value_index].count_is_set != 0 &&
					wd->step_result[step->value_index].count > 0) {
				if (new_src_ptr_val != NULL) {
					msg_stddbg("freeing for %s: %p\n", step->global_path, new_src_ptr_val);
					free(new_src_ptr_val);
					*((void**)(wd->ptr + step->offset)) = NULL;
				}
			}
		}
	}
	free(ptr);
	msg_stddbg("========================\n");

	/*TODO: free everything */
	cds_list_for_each_entry_safe(wd, _wd, &data.memobjwd_list, list) {
		cds_list_del(&wd->list);
		delete_wd(wd);
	}

	return 0;
}

//int metac_copy(metac_precompiled_type_t * precompiled_type, void *ptr, metac_byte_size_t element_size/*important for types with flex arrays??? can't do arrays of such type*/, int n, void **p_ptr) {
int metac_copy(metac_precompiled_type_t * precompiled_type, void *ptr, metac_byte_size_t size, void **p_ptr, metac_byte_size_t * p_size) {
	int i;
	struct memobj_work_data* wd, *_wd;
	struct precomiled_type_work_data data;

	if (precompiled_type == NULL) {
		msg_stderr("invalid argument value: precompiled_type\n");
		return -(EINVAL);
	}

	data.precompiled_type = precompiled_type;
	CDS_INIT_LIST_HEAD(&data.memobjwd_list);

	/* init first wd */
	wd = create_wd(data.precompiled_type->memobj[0], ptr, size);
	if (wd == NULL) {
		return -(ENOMEM);
	}
	wd->step_result[0].temp_data = calloc(1, size);
	memcpy(wd->step_result[0].temp_data, ptr, size);
	msg_stddbg("%p\n", wd->step_result[0].temp_data);

	cds_list_add_tail(&wd->list, &data.memobjwd_list);

	/* go width ip (pointers/arrays) */
	cds_list_for_each_entry(wd, &data.memobjwd_list, list) {
		struct memobj * memobj = wd->memobj;

		/* init results*/
		if (_init_conditions(&data, wd) != 0) {
			return -(EFAULT);
		}

		for (i = memobj->pointer_type_idx; i < memobj->pointer_type_idx+memobj->pointer_type_steps_count; i++) {
			struct step * step = memobj->step[i];
			int j;
			wd->step_result[step->value_index].count_is_set = 1;
			wd->step_result[step->value_index].count = 0;

			if (_visitor_check_condition(wd, step) != 0)
				continue;

			/*TODO: store counter - and reuse it next time like conditions*/
			/*get count*/
			switch(step->array_mode) {
			case amStop: continue;	/*don't go here */
			case amExtendAsOneObject: {
				wd->step_result[step->value_index].count = 1;
			} break;
			case amExtendAsArrayWithLen:
				assert(step->array_elements_count_funtion_ptr != NULL);
				if (step->array_elements_count_funtion_ptr(0,
						step->type, step->context, wd->ptr, 1,
						&wd->step_result[step->value_index].count) != 0) {
					msg_stderr("array_elements_count_funtion failed\n");
					continue;
				}
				break;
			case amExtendAsArrayWithNullEnd:
				wd->step_result[step->value_index].count_is_set = 0;
				break;
			}

			msg_stddbg("ptr: %s mode %d count %d\n", step->global_path, (int)step->array_mode, wd->step_result[step->value_index].count);

			/*and go to another memobj*/
			if (	wd->step_result[step->value_index].count_is_set != 0 &&
					wd->step_result[step->value_index].count > 0) {

				struct memobj_work_data* new_wd;
				struct memobj * next_memobj = data.precompiled_type->memobj[step->memobj_idx];
				void *new_src_ptr_val = *((void**)(wd->ptr + step->offset));	/*read pointer value*/
				metac_byte_size_t new_byte_size = metac_type_byte_size(data.precompiled_type->memobj[step->memobj_idx]->type);
				void *allocated_dst_mem;

				if (new_src_ptr_val == NULL) {
					msg_stderr("met NULL\n");
					continue;
				}

				allocated_dst_mem = calloc(wd->step_result[step->value_index].count, new_byte_size);
				memcpy(allocated_dst_mem, new_src_ptr_val, wd->step_result[step->value_index].count * new_byte_size);
				*((void**)(wd->step_result[0].temp_data + step->offset)) = allocated_dst_mem;
				msg_stddbg("allocated for %s: %p\n", step->global_path, allocated_dst_mem);

				msg_stddbg("ptr: %s count %d\n", step->global_path, wd->step_result[step->value_index].count);
				for (j = 0; j < wd->step_result[step->value_index].count;  j++) { /* add array to queue */
					/*TODO: check if already had this addr in range [new_ptr + j * new_byte_size, new_byte_size) - loop*/

					new_wd = create_wd(next_memobj, new_src_ptr_val + j * new_byte_size, new_byte_size);
					if (new_wd == NULL) {
						msg_stderr("memobj_idx is too big\n");
						return -(ENOMEM);
					}
					new_wd->step_result[0].temp_data = allocated_dst_mem + j * new_byte_size;
					cds_list_add_tail(&new_wd->list, &data.memobjwd_list);
				}
			}/*else*/

			/*visit base types */
		}

	}
	msg_stddbg("========================\n");

	/*returning result*/
	wd = cds_list_first_entry(&data.memobjwd_list, struct memobj_work_data, list);
	msg_stddbg("%p\n", wd->step_result[0].temp_data);
	if (p_ptr != NULL) {
		*p_ptr = wd->step_result[0].temp_data;
	}
	if (p_size != NULL) {
		*p_size = size;
	}

	/*TODO: free everything */
	cds_list_for_each_entry_safe(wd, _wd, &data.memobjwd_list, list) {
		cds_list_del(&wd->list);
		delete_wd(wd);
	}

	return 0;
}
#endif
