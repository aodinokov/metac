/*
 * metac_precompile_type.c
 *
 *  Created on: Nov 28, 2017
 *      Author: mralex
 */
//#define METAC_DEBUG_ENABLE
#include <stdlib.h>			/* calloc, ... */
#include <string.h>			/* strlen, strcpy */
#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <urcu/list.h>		/* I like struct cds_list_head :) */

#include "metac_type.h"
#include "metac_debug.h"			/* msg_stderr, ...*/
#include "metac_internals.h"		/*definitions of internal objects*/
#include "breadthfirst_engine.h"	/*breadthfirst_engine module*/

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
/*****************************************************************************/
/*temporary types for types pre-compilation*/
/*****************************************************************************/
struct _discriminator {
	struct cds_list_head list;

	struct discriminator * p_discriminator;
};

struct _region_element_type {
	struct cds_list_head list;

	struct region_element_type * p_region_element_type;
	struct cds_list_head discriminator_list;
};

struct precompile_context {
	struct traversing_engine traversing_engine;
	metac_precompiled_type_t * precompiled_type;

	struct cds_list_head executed_tasks_queue;
	struct cds_list_head region_element_type_list;	/*current list of all created region types for precompiled type */
};

struct precompile_task {
	struct traversing_engine_task task;

	struct precompile_task* parent_task;
	struct _region_element_type * _region_element_type;
	struct metac_type *type;
	struct condition precondition;
	char *	name_local;
	char *	given_name_local;
	metac_data_member_location_t offset;
	metac_bit_offset_t * p_bit_offset;
	metac_bit_size_t * p_bit_size;
	metac_byte_size_t byte_size;

	/* runtime data */
	struct metac_type *actual_type;
	struct region_element_type_element * region_element_type_element;
};
/*****************************************************************************/
static struct _region_element_type * create__region_element_type(
		struct metac_type * type) {
	struct _region_element_type * _region_element_type;

	_region_element_type = calloc(1, sizeof(*_region_element_type));
	if (_region_element_type == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	_region_element_type->p_region_element_type = create_region_element_type(type);
	if (_region_element_type->p_region_element_type == NULL) {
		msg_stderr("create_region_element_type failed\n");
		free(_region_element_type);
		return NULL;
	}
	CDS_INIT_LIST_HEAD(&_region_element_type->discriminator_list);

	return _region_element_type;
}

static struct _region_element_type * simply_create__region_element_type(
		struct precompile_context * p_precompile_context,
		struct metac_type * type ) {
	struct _region_element_type * _region_element_type = NULL;
	/*create otherwise*/
	msg_stddbg("create region_element_type for : %s\n", type->name);
	_region_element_type = create__region_element_type(type);
	msg_stddbg("create region_element_type result %p\n", _region_element_type->p_region_element_type);
	if (_region_element_type == NULL) {
		msg_stddbg("create__region_element_type failed\n");
		return NULL;
	}
	cds_list_add_tail(&_region_element_type->list, &p_precompile_context->region_element_type_list);
	++p_precompile_context->precompiled_type->region_element_types_count;
	return _region_element_type;
}

static struct _region_element_type * find_or_create__region_element_type(
		struct precompile_context * p_precompile_context,
		struct metac_type * type,
		int * p_created_flag) {
	/*check if region_element_type for the same type already exists*/
	struct _region_element_type * _region_element_type = NULL;
	struct _region_element_type * _region_element_type_iter;

	assert(type->id != DW_TAG_typedef && type->id != DW_TAG_const_type);

	if (p_created_flag != NULL) *p_created_flag = 0;

	cds_list_for_each_entry(_region_element_type_iter, &p_precompile_context->region_element_type_list, list) {
		if (_region_element_type_iter->p_region_element_type->type == type){
			_region_element_type = _region_element_type_iter;
			msg_stddbg("found %p\n", _region_element_type);
			break;
		}
	}
	if (_region_element_type == NULL) {
		_region_element_type = simply_create__region_element_type(p_precompile_context, type);
		if (_region_element_type != NULL) {
			if (p_created_flag != NULL) *p_created_flag = 1;
		}
	}

	return _region_element_type;
}
/*****************************************************************************/
static struct _discriminator * create__discriminator(
		struct _region_element_type * _region_element_type,
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

	cds_list_add(&_discriminator->list, &_region_element_type->discriminator_list);
	++_region_element_type->p_region_element_type->discriminators_count;
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

static struct precompile_task* create_and_add_precompile_task_to_front(
		struct traversing_engine* p_traversing_engine,
		struct precompile_task * parent_task,
		struct _region_element_type * _region_element_type,
		struct metac_type * type,
		traversing_engine_task_fn_t fn,
		struct discriminator * p_discriminator,
		metac_discriminator_value_t expected_discriminator_value,
		char * name_local,
		char * given_name_local,
		metac_data_member_location_t offset,
		metac_bit_offset_t * p_bit_offset,
		metac_bit_size_t * p_bit_size,
		metac_byte_size_t byte_size) {
	struct precompile_task* p_task;

	msg_stddbg("(name_local = %s, given_name_local = %s, offset = %d, byte_size = %d)\n",
			name_local, given_name_local,
			(int)offset, (int)byte_size);

	/* allocate object */
	p_task = calloc(1, sizeof(*p_task));
	if (p_task == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	p_task->task.fn = fn;

	p_task->parent_task = parent_task;
	p_task->_region_element_type = _region_element_type;
	p_task->type = type;
	p_task->name_local = name_local!=NULL?strdup(name_local):NULL;
	p_task->given_name_local = given_name_local!=NULL?strdup(given_name_local):NULL;
	p_task->offset = offset;
	p_task->p_bit_offset = p_bit_offset;
	p_task->p_bit_size = p_bit_size;
	p_task->byte_size = byte_size;

	if (p_discriminator != NULL) {	/*copy precondition*/
		p_task->precondition.p_discriminator = p_discriminator;
		p_task->precondition.expected_discriminator_value = expected_discriminator_value;
	}

	if (add_traversing_task_to_front(p_traversing_engine, &p_task->task) != 0) {
		msg_stderr("add_breadthfirst_task failed\n");
		free(p_task);
		return NULL;
	}
	return p_task;
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
static int _parse_type_task(
		struct traversing_engine * p_traversing_engine,
		struct traversing_engine_task * p_traversing_engine_task,
		int error_flag){
	/*TODO: check spec on global level - e.g. that will allow to make Stop for some type*/
	char * path_global;
	char * path_within_region;
	char * parent_path_global;
	char * parent_path_within_region;
	struct precompile_context * p_precompile_context = cds_list_entry(p_traversing_engine, struct precompile_context, traversing_engine);
	struct precompile_task * p_precompile_task = cds_list_entry(p_traversing_engine_task, struct precompile_task, task);

	cds_list_add_tail(&p_traversing_engine_task->list, &p_precompile_context->executed_tasks_queue);
	if (error_flag != 0) return 0;

	/* get actual type */
	p_precompile_task->actual_type = get_actual_type(p_precompile_task->type);

	/* generate paths */
	if (	p_precompile_task->parent_task != NULL &&
			p_precompile_task->parent_task->region_element_type_element != NULL &&
			p_precompile_task->parent_task->region_element_type_element->path_global != NULL) {
		parent_path_global = p_precompile_task->parent_task->region_element_type_element->path_global;
		parent_path_within_region = p_precompile_task->parent_task->_region_element_type == p_precompile_task->_region_element_type?
				p_precompile_task->parent_task->region_element_type_element->path_within_region_element:
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

	/*create struct region_element_type_element in our region_element_type based on the data from task*/
	p_precompile_task->region_element_type_element = create_region_element_type_element(
			get_actual_type(p_precompile_task->type),
			p_precompile_task->precondition.p_discriminator, p_precompile_task->precondition.expected_discriminator_value,
			p_precompile_task->offset, p_precompile_task->byte_size,
			p_precompile_task->parent_task != NULL?
					p_precompile_task->parent_task->_region_element_type == p_precompile_task->_region_element_type?
							p_precompile_task->parent_task->region_element_type_element:
							NULL:
					NULL,
			p_precompile_task->name_local, path_within_region, path_global, NULL, NULL, NULL);

	free(path_global);
	free(path_within_region);

	if (p_precompile_task->region_element_type_element == NULL) {
		msg_stddbg("failed to create region_element_type_element\n");
		return -EFAULT;
	}

	/* generate children tasks */
	switch(p_precompile_task->actual_type->id) {
	case DW_TAG_structure_type: {
		metac_type_t * type = p_precompile_task->actual_type;
		int is_anon;
		int anon_members_count = 0;
		metac_num_t i, _i;
		for (_i = 0; _i < type->structure_type_info.members_count; _i++) {
			i = type->structure_type_info.members_count - _i -1;
			char anon_name[15];
			is_anon = 0;
			if (strcmp(type->structure_type_info.members[i].name, "") == 0) {
				is_anon = 1;
				snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
			}
			if (create_and_add_precompile_task_to_front(
					p_traversing_engine,
					p_precompile_task,
					p_precompile_task->_region_element_type,
					type->structure_type_info.members[i].type,
					_parse_type_task,
					p_precompile_task->precondition.p_discriminator, p_precompile_task->precondition.expected_discriminator_value,
					type->structure_type_info.members[i].name,
					is_anon?anon_name:type->structure_type_info.members[i].name,
					p_precompile_task->offset + type->structure_type_info.members[i].data_member_location,
					type->structure_type_info.members[i].p_bit_offset,
					type->structure_type_info.members[i].p_bit_size,
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
		metac_num_t i, _i;
		/* try to find discriminator ptr */
		const metac_type_specification_value_t * spec =
				metac_type_specification(p_precompile_context->precompiled_type->type, p_precompile_task->region_element_type_element->path_global);
		/*TODO: we can check p_precompile_task->type */
		if (spec == NULL || spec->discriminator_funtion_ptr == NULL) {
			msg_stddbg("Warning: Union %s doesn't have a union-type specification - skipping its children\n",
					p_precompile_task->region_element_type_element->path_global);
			/*TODO: mark to skip the step */
			break;
		}
		/*we have new discriminator in spec - let's create it*/
		_discriminator = create__discriminator(p_precompile_task->_region_element_type,
				p_precompile_task->precondition.p_discriminator, p_precompile_task->precondition.expected_discriminator_value,
				spec->discriminator_funtion_ptr,
				spec->specification_context);
		if (_discriminator == NULL) {
			return -EFAULT;
		}

		for (_i = 0; _i < type->union_type_info.members_count; _i++) {
			i = type->union_type_info.members_count - _i - 1;
			char anon_name[15];

			is_anon = 0;
			if (strcmp(type->union_type_info.members[i].name, "") == 0) {
				is_anon = 1;
				snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
			}
			if (create_and_add_precompile_task_to_front(
					p_traversing_engine,
					p_precompile_task,
					p_precompile_task->_region_element_type,
					type->union_type_info.members[i].type,
					_parse_type_task,
					_discriminator->p_discriminator, i,
					type->union_type_info.members[i].name,
					is_anon?anon_name:type->union_type_info.members[i].name,
					p_precompile_task->offset + type->union_type_info.members[i].data_member_location,
					type->union_type_info.members[i].p_bit_offset,
					type->union_type_info.members[i].p_bit_size,
					metac_type_byte_size(type->union_type_info.members[i].type)) == NULL) {
				msg_stderr("create_and_add_precompile_task failed\n");
				return -EFAULT;
			}
		}
	} break;
	case DW_TAG_array_type:
	case DW_TAG_pointer_type: {
		int new_region_was_created = 0;
		struct _region_element_type * array_elements__region_element_type = NULL;
		metac_type_t * array_elements_type = NULL;

		const metac_type_specification_value_t * spec = metac_type_specification(
				p_precompile_context->precompiled_type->type, p_precompile_task->region_element_type_element->path_global);
		metac_type_specification_value_t default_spec = {
			.discriminator_funtion_ptr = NULL,
			.array_elements_count_funtion_ptr = NULL,
			.specification_context = NULL,
		};

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
			msg_stddbg("Warning: The type at %s can't be parsed further - skipping its children\n",
					p_precompile_task->region_element_type_element->path_global);
			return 0;
		}

		if (p_precompile_task->actual_type->id == DW_TAG_pointer_type || (
				p_precompile_task->actual_type->id == DW_TAG_array_type &&
				p_precompile_task->actual_type->array_type_info.is_flexible)) {
			if (spec == NULL || spec->array_elements_count_funtion_ptr == NULL) {
				/*try to find better defaults:
				 * for char* - metac_array_elements_1d_with_null
				 * for anyothertyep* - metac_array_elements_single
				 */
				if (p_precompile_task->actual_type->id == DW_TAG_pointer_type &&
					array_elements_type->id == DW_TAG_base_type &&
					array_elements_type->name != NULL &&
					strcmp(array_elements_type->name, "char")==0) {
					default_spec.array_elements_count_funtion_ptr = metac_array_elements_1d_with_null;
					spec = &default_spec;
				}else if (p_precompile_task->actual_type->id == DW_TAG_pointer_type) {
					default_spec.array_elements_count_funtion_ptr = metac_array_elements_single;
					spec = &default_spec;
				}else {
					msg_stddbg("Warning: Can't get array/pointer spec at %s - skipping its children\n",
							p_precompile_task->region_element_type_element->path_global);
					return 0;
				}
			}
		}

		if (p_precompile_task->actual_type->id == DW_TAG_pointer_type) {
			array_elements__region_element_type = find_or_create__region_element_type(p_precompile_context, get_actual_type(array_elements_type), &new_region_was_created);
		}else {
			array_elements__region_element_type = simply_create__region_element_type(p_precompile_context, get_actual_type(array_elements_type));
			new_region_was_created = 1;
		}
		if (array_elements__region_element_type == NULL) {
			msg_stderr("ERROR: cant create region_element_type - exiting\n");
			return -EFAULT;
		}

		msg_stddbg("p_precompile_task %p array_elements__region_element_type %p\n", p_precompile_task, array_elements__region_element_type);
		update_region_element_type_element_array_params(
				p_precompile_task->region_element_type_element,
				spec?spec->array_elements_count_funtion_ptr:NULL,
				spec?spec->specification_context:NULL,
				array_elements__region_element_type->p_region_element_type);

		if (new_region_was_created != 0) {
			if (create_and_add_precompile_task_to_front(
					p_traversing_engine,
					p_precompile_task,
					array_elements__region_element_type,
					array_elements_type,
					_parse_type_task,
					NULL, 0,
					"",
					"<ptr>", //p_precompile_task->actual_type->id == DW_TAG_pointer_type?"<ptr>":"<a_elem_n>", /*may be it's better to keep ptr for all cases*/
					0,
					NULL,
					NULL,
					metac_type_byte_size(array_elements_type)) == NULL) {
				msg_stderr("create_and_add_precompile_task failed\n");
				return -EFAULT;
			}
		}
	}break;
	}

	return 0;
}

/*****************************************************************************/
static int _phase2_calc_elements_per_type(
		struct precompile_context *p_precompile_context,
		struct traversing_engine_task * p_traversing_engine_task) {
	struct precompile_task * p_precompile_task = cds_list_entry(p_traversing_engine_task, struct precompile_task, task);

	switch (p_precompile_task->actual_type->id) {
		case DW_TAG_base_type:
			++p_precompile_task->_region_element_type->p_region_element_type->base_type_elements_count;
			break;
		case DW_TAG_enumeration_type:
			++p_precompile_task->_region_element_type->p_region_element_type->enum_type_elements_count;
			break;
		case DW_TAG_pointer_type:
			++p_precompile_task->_region_element_type->p_region_element_type->pointer_type_elements_count;
			break;
		case DW_TAG_array_type:
			++p_precompile_task->_region_element_type->p_region_element_type->array_type_elements_count;
			break;
		default:
			++p_precompile_task->_region_element_type->p_region_element_type->hierarchy_elements_count;
	}
	++p_precompile_task->_region_element_type->p_region_element_type->elements_count;
	return 0;
}

static int _phase2_put_elements_per_type(
		struct precompile_context *p_precompile_context,
		struct traversing_engine_task * p_traversing_engine_task) {
	struct precompile_task * p_precompile_task = cds_list_entry(p_traversing_engine_task, struct precompile_task, task);

	switch (p_precompile_task->actual_type->id) {
		case DW_TAG_base_type:
			p_precompile_task->_region_element_type->p_region_element_type->base_type_element[p_precompile_task->_region_element_type->p_region_element_type->base_type_elements_count] = p_precompile_task->region_element_type_element;
			++p_precompile_task->_region_element_type->p_region_element_type->base_type_elements_count;
			break;
		case DW_TAG_enumeration_type:
			p_precompile_task->_region_element_type->p_region_element_type->enum_type_element[p_precompile_task->_region_element_type->p_region_element_type->enum_type_elements_count] = p_precompile_task->region_element_type_element;
			++p_precompile_task->_region_element_type->p_region_element_type->enum_type_elements_count;
			break;
		case DW_TAG_pointer_type:
			p_precompile_task->_region_element_type->p_region_element_type->pointer_type_element[p_precompile_task->_region_element_type->p_region_element_type->pointer_type_elements_count] = p_precompile_task->region_element_type_element;
			++p_precompile_task->_region_element_type->p_region_element_type->pointer_type_elements_count;
			break;
		case DW_TAG_array_type:
			p_precompile_task->_region_element_type->p_region_element_type->array_type_element[p_precompile_task->_region_element_type->p_region_element_type->array_type_elements_count] = p_precompile_task->region_element_type_element;
			++p_precompile_task->_region_element_type->p_region_element_type->array_type_elements_count;
			break;
		default:
			p_precompile_task->_region_element_type->p_region_element_type->hierarchy_element[p_precompile_task->_region_element_type->p_region_element_type->hierarchy_elements_count] = p_precompile_task->region_element_type_element;
			++p_precompile_task->_region_element_type->p_region_element_type->hierarchy_elements_count;
	}
	p_precompile_task->_region_element_type->p_region_element_type->element[p_precompile_task->_region_element_type->p_region_element_type->elements_count] = p_precompile_task->region_element_type_element;
	p_precompile_task->region_element_type_element->id = p_precompile_task->_region_element_type->p_region_element_type->elements_count;
	++p_precompile_task->_region_element_type->p_region_element_type->elements_count;
	return 0;
}

static int _phase2(
		struct precompile_context *p_precompile_context) {
	int i = 0;
	struct traversing_engine_task * task;
	struct _region_element_type * _region_element_type;
	struct _discriminator * _discriminator;

	/* allocate memory for region_element_type array*/
	p_precompile_context->precompiled_type->region_element_type = calloc(p_precompile_context->precompiled_type->region_element_types_count, sizeof(*p_precompile_context->precompiled_type->region_element_type));
	if (p_precompile_context->precompiled_type->region_element_type == NULL) {
		msg_stderr("Can't allocate memory for region_element_type\n");
		return -ENOMEM;
	}
	/* collect region_element_types and their discriminators */
	cds_list_for_each_entry(_region_element_type, &p_precompile_context->region_element_type_list, list) {
		int j = 0;

		p_precompile_context->precompiled_type->region_element_type[i] = _region_element_type->p_region_element_type;

		p_precompile_context->precompiled_type->region_element_type[i]->discriminator =
				calloc(p_precompile_context->precompiled_type->region_element_type[i]->discriminators_count,
						sizeof(*p_precompile_context->precompiled_type->region_element_type[i]->discriminator));
		if (p_precompile_context->precompiled_type->region_element_type[i]->discriminator == NULL) {
			msg_stderr("Can't allocate memory for region_element_type->discriminator\n");
			return -ENOMEM;
		}
		cds_list_for_each_entry(_discriminator, &_region_element_type->discriminator_list, list) {
			_discriminator->p_discriminator->id = j;
			p_precompile_context->precompiled_type->region_element_type[i]->discriminator[j] = _discriminator->p_discriminator;
			j++;
		}
		i++;
	}

	cds_list_for_each_entry(task, &p_precompile_context->executed_tasks_queue, list) {
		_phase2_calc_elements_per_type(p_precompile_context, task);
	}

	for (i = 0; i < p_precompile_context->precompiled_type->region_element_types_count; i++) {
		p_precompile_context->precompiled_type->region_element_type[i]->element = calloc(
				p_precompile_context->precompiled_type->region_element_type[i]->elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_element_type[i]->element));
		p_precompile_context->precompiled_type->region_element_type[i]->base_type_element = calloc(
				p_precompile_context->precompiled_type->region_element_type[i]->base_type_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_element_type[i]->base_type_element));
		p_precompile_context->precompiled_type->region_element_type[i]->enum_type_element = calloc(
				p_precompile_context->precompiled_type->region_element_type[i]->enum_type_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_element_type[i]->enum_type_element));
		p_precompile_context->precompiled_type->region_element_type[i]->array_type_element = calloc(
				p_precompile_context->precompiled_type->region_element_type[i]->array_type_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_element_type[i]->array_type_element));
		p_precompile_context->precompiled_type->region_element_type[i]->pointer_type_element = calloc(
				p_precompile_context->precompiled_type->region_element_type[i]->pointer_type_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_element_type[i]->pointer_type_element));
		p_precompile_context->precompiled_type->region_element_type[i]->hierarchy_element = calloc(
				p_precompile_context->precompiled_type->region_element_type[i]->hierarchy_elements_count,
				sizeof(*p_precompile_context->precompiled_type->region_element_type[i]->hierarchy_element));
		if (	(p_precompile_context->precompiled_type->region_element_type[i]->elements_count > 0 && p_precompile_context->precompiled_type->region_element_type[i]->element == NULL)
			||	(p_precompile_context->precompiled_type->region_element_type[i]->base_type_elements_count > 0 && p_precompile_context->precompiled_type->region_element_type[i]->base_type_element == NULL)
			||	(p_precompile_context->precompiled_type->region_element_type[i]->enum_type_elements_count > 0 && p_precompile_context->precompiled_type->region_element_type[i]->enum_type_element == NULL)
			||	(p_precompile_context->precompiled_type->region_element_type[i]->array_type_elements_count > 0 && p_precompile_context->precompiled_type->region_element_type[i]->array_type_element == NULL)
			||	(p_precompile_context->precompiled_type->region_element_type[i]->pointer_type_elements_count > 0 && p_precompile_context->precompiled_type->region_element_type[i]->pointer_type_element == NULL)
			||	(p_precompile_context->precompiled_type->region_element_type[i]->hierarchy_elements_count > 0 && p_precompile_context->precompiled_type->region_element_type[i]->hierarchy_element == NULL)
			) {
			msg_stderr("Can't allocate memory for pointers of elements\n");
			return -ENOMEM;
		}
		p_precompile_context->precompiled_type->region_element_type[i]->elements_count = 0;
		p_precompile_context->precompiled_type->region_element_type[i]->base_type_elements_count = 0;
		p_precompile_context->precompiled_type->region_element_type[i]->enum_type_elements_count = 0;
		p_precompile_context->precompiled_type->region_element_type[i]->array_type_elements_count = 0;
		p_precompile_context->precompiled_type->region_element_type[i]->pointer_type_elements_count = 0;
		p_precompile_context->precompiled_type->region_element_type[i]->hierarchy_elements_count = 0;
	}

	cds_list_for_each_entry(task, &p_precompile_context->executed_tasks_queue, list) {
		_phase2_put_elements_per_type(p_precompile_context, task);
	}

	return 0;
}
/*****************************************************************************/
static void cleanup_precompile_context(struct precompile_context *p_precompile_context) {
	struct traversing_engine_task * task, *_task;
	struct _region_element_type * _region_element_type, * __region_element_type;
	struct _discriminator * _discriminator, * __discriminator;

	cleanup_traversing_engine(&p_precompile_context->traversing_engine);

	cds_list_for_each_entry_safe(task, _task, &p_precompile_context->executed_tasks_queue, list) {
		struct precompile_task * p_precompile_task = cds_list_entry(task, struct precompile_task, task);
		cds_list_del(&task->list);
		delete_precompile_task(&p_precompile_task);
	}

	cds_list_for_each_entry_safe(_region_element_type, __region_element_type, &p_precompile_context->region_element_type_list, list) {
		cds_list_for_each_entry_safe(_discriminator, __discriminator, &_region_element_type->discriminator_list, list) {
			cds_list_del(&_discriminator->list);
			free(_discriminator);
		}
		cds_list_del(&_region_element_type->list);
		free(_region_element_type);
	}
}
/*****************************************************************************/
metac_precompiled_type_t * metac_precompile_type(struct metac_type *type) {
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
	CDS_INIT_LIST_HEAD(&context.executed_tasks_queue);
	CDS_INIT_LIST_HEAD(&context.region_element_type_list);

	/*use breadthfirst_engine*/
	if (init_traversing_engine(&context.traversing_engine) != 0){
		msg_stderr("create_breadthfirst_engine failed\n");
		delete_precompiled_type(&context.precompiled_type);
		return NULL;
	}

	if (create_and_add_precompile_task_to_front(&context.traversing_engine, NULL, find_or_create__region_element_type(&context, get_actual_type(type), NULL),
			type,
			_parse_type_task,
			NULL, 0, "", "<ptr>", 0, NULL, NULL, metac_type_byte_size(type)) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");
		/*TBD: test if it's ok - not sure, because some of objects are not linked at that time*/
		cleanup_precompile_context(&context);
		delete_precompiled_type(&context.precompiled_type);
		return NULL;
	}
	if (run_traversing_engine(&context.traversing_engine) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");
		cleanup_precompile_context(&context);
		delete_precompiled_type(&context.precompiled_type);
		return NULL;
	}

	if (_phase2(&context) != 0) {
		msg_stderr("Phase 2 returned error\n");
		cleanup_precompile_context(&context);
		delete_precompiled_type(&context.precompiled_type);
		return NULL;
	}

	cleanup_precompile_context(&context);
	return context.precompiled_type;
}
