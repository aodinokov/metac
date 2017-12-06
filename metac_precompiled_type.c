/*
 * metac_precompiled_type.c
 *
 *  Created on: Nov 28, 2017
 *      Author: mralex
 */

#include "metac_type.h"
#include "metac_debug.h"	/* msg_stderr, ...*/

#include <stdlib.h>			/* calloc, ... */
#include <string.h>			/* strlen, strcpy */
#include <assert.h>			/* assert */
#include <errno.h>			/* ENOMEM etc */
#include <urcu/list.h>		/* I like struct cds_list_head :) */

struct condition;
struct condition_check {
	struct condition *p_condition;
	int expected_value;
};

struct step {
	struct metac_type * type;	/*type without typedef*/
	char * name;
	char * global_path;
	char * path;
	metac_data_member_location_t 	offset;
	metac_byte_size_t 				byte_size;

	struct step	* parent;
	struct condition_check check;

	/* if ptr or arrays */
	enum metac_array_mode array_mode; /*can be - always 1, and check fn. may be better to put spec ptr here */
	metac_array_elements_count_funtion_ptr_t array_elements_count_funtion_ptr;	/*if ptr or flexible array - overrides byte_size*/
	void * context;
	int memobj_idx;				/*if ptr or flrxible array*/

	/* if struct and etc */
	int is_anon;
	/*need to store children*/

	int 	value_index; /*values will be stored in the array - will be initialized later*/
};

struct condition {
	struct condition_check check;
	metac_discriminator_funtion_ptr_t condition_fn_ptr;
	void * context;
};

struct memobj {
	struct metac_type *type;

	int conditions_count;
	struct condition ** condition;

	int steps_count;
	struct step ** step;

	int base_type_idx;
	int base_type_steps_count;
	int enum_type_idx;
	int enum_type_steps_count;
	int pointer_type_idx;
	int pointer_type_steps_count;
};

struct metac_precompiled_type {
	struct metac_type *type;
	int	memobjs_count;
	struct memobj **memobj;
};

/*****************************************************************************/
/*temporary types for phase 1*/
/*****************************************************************************/
struct _condition;

struct _step {
	struct cds_list_head list;
	int memobj_id;
	struct step * p_step;
};

struct _condition {
	struct cds_list_head list;
	int memobj_id;
	struct condition *p_condition;
};

struct _memobj {
	struct cds_list_head list;
	int memobj_id;
	struct memobj *p_memobj;
};

/*****************************************************************************/
int _init_path(char**p_path, char *path, char *name) {
	size_t path_len = path?(strlen(path)+1):0;
	size_t name_len = name?(strlen(name)):0;

	(*p_path) = calloc(1, path_len + name_len + 1 /*"\0"*/);
	if ((*p_path) == NULL) {
		msg_stderr("no memory\n");
		return -1;
	}

	if (path) {
		strcpy((*p_path), path);
		if (name_len > 0)
			strcpy(&(*p_path)[path_len - 1], ".");
	}
	strcpy(&(*p_path)[path_len], name);
	return 0;
}

struct _step * create__step(
		int memobj_id,
		char *global_path, char *global_generated_name, char *path, char *generated_name, char *name,
		struct metac_type *type,
		metac_data_member_location_t offset,
		metac_byte_size_t byte_size,
		struct _step * parent) {
	struct _step *_step;

	if (name == NULL || type == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}

	/* allocate object */
	_step = calloc(1, sizeof(*_step));
	if (_step == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	_step->p_step = calloc(1, sizeof(*(_step->p_step)));
	if (_step->p_step == NULL) {
		msg_stderr("no memory\n");
		free(_step);
		return NULL;
	}

	/* save type without typedefs*/
	_step->p_step->type = metac_type_typedef_skip(type);

	/* generate and save path */
	if (_init_path(&_step->p_step->path, path, generated_name) != 0) {
		free(_step->p_step);
		free(_step);
		return NULL;
	}
	if (_init_path(&_step->p_step->global_path, global_path, global_generated_name) != 0) {
		free(_step->p_step->path);
		free(_step->p_step);
		free(_step);
		return NULL;
	}
	_step->p_step->name = name!=NULL?strdup(name):NULL;
	if (_step->p_step->name == NULL) {
		free(_step->p_step->global_path);
		free(_step->p_step->path);
		free(_step->p_step);
		free(_step);
	}

	/*save offset/byte_size*/
	_step->p_step->offset = offset;
	_step->p_step->byte_size = byte_size;

	_step->memobj_id = memobj_id;

	if (parent != NULL) {
		_step->p_step->check.p_condition = parent->p_step->check.p_condition;
		_step->p_step->check.expected_value = parent->p_step->check.expected_value;
	}

	/*init defaults*/
	_step->p_step->memobj_idx = -1;

	return _step;
}

void delete_step(struct step * p_step) {
	if (p_step) {
		if (p_step->path) {
			free(p_step->path);
			p_step->path = NULL;
		}
		if (p_step->global_path) {
			free(p_step->global_path);
			p_step->global_path = NULL;
		}
		if (p_step->name) {
			free(p_step->name);
			p_step->name = NULL;
		}
		free(p_step);
	}
}

void delete__step(struct _step * _step) {
	if (_step) {
		delete_step(_step->p_step);
		_step->p_step = NULL;
		free(_step);
	}
}

struct _condition * create__condition(
		int memobj_id,
		metac_discriminator_funtion_ptr_t condition_fn_ptr,
		void * context,
		struct _step * parent_step) {
	struct _condition *_condition;

	/* allocate object */
	_condition = calloc(1, sizeof(*_condition));
	if (_condition == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	_condition->p_condition = calloc(1, sizeof(*(_condition->p_condition)));
	if (_condition->p_condition == NULL) {
		msg_stderr("no memory\n");
		free(_condition);
		return NULL;
	}

	_condition->memobj_id = memobj_id;
	_condition->p_condition->condition_fn_ptr = condition_fn_ptr;
	_condition->p_condition->context = context;

	/*copy check from parent*/
	_condition->p_condition->check.p_condition = parent_step->p_step->check.p_condition;
	_condition->p_condition->check.expected_value = parent_step->p_step->check.expected_value;

	return _condition;
}

void delete_condition(struct condition *p_condition) {
	if (p_condition) {
		free(p_condition);
	}
}

void delete__condition(struct _condition *_condition) {
	if (_condition) {
		delete_condition(_condition->p_condition);
		free(_condition);
	}
}

struct _memobj * create__memobj(
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

void delete_memobj(struct memobj *p_memobj) {
	if (p_memobj) {
		free(p_memobj);
	}
}

void delete__memobj(struct _memobj *_memobj) {
	if (_memobj) {
		delete_memobj(_memobj->p_memobj);
		free(_memobj);
	}
}

struct _memobj * _find_memobj_by_type(
		struct cds_list_head * memobjs_list,
		metac_type_t * type) {
	struct _memobj * _memobj;

	cds_list_for_each_entry(_memobj, memobjs_list, list) {
		if (_memobj->p_memobj->type == type)
			return _memobj;
	}
	return NULL;
}

struct _memobj * _find_memobj_by_id(
		struct cds_list_head * memobjs_list,
		int memobj_id) {
	struct _memobj * _memobj;

	cds_list_for_each_entry(_memobj, memobjs_list, list) {
		if (_memobj->memobj_id == memobj_id)
			return _memobj;
	}
	return NULL;
}

int _phase1(
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
		case DW_TAG_array_type: {
			/*TODO: flex and not flex cases */
			/*make it as a pointer, but offset should be changed */
		} break;
		}
	}
	return 0;
}

void _free_phase1_lists(
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

int _phase2(
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
			msg_stddbg("all %d p %d b %d e %d\n",
					precompiled_type->memobj[i].steps_count,
					precompiled_type->memobj[i].pointer_type_steps_count,
					precompiled_type->memobj[i].base_type_steps_count,
					precompiled_type->memobj[i].enum_type_steps_count);
#endif
			/*put leafs to the end of array*/
			precompiled_type->memobj[i]->pointer_type_idx = precompiled_type->memobj[i]->steps_count -
					precompiled_type->memobj[i]->pointer_type_steps_count -
					precompiled_type->memobj[i]->base_type_steps_count -
					precompiled_type->memobj[i]->enum_type_steps_count;
			precompiled_type->memobj[i]->base_type_idx = precompiled_type->memobj[i]->pointer_type_idx +
					precompiled_type->memobj[i]->pointer_type_steps_count;
			precompiled_type->memobj[i]->enum_type_idx = precompiled_type->memobj[i]->base_type_idx +
					precompiled_type->memobj[i]->base_type_steps_count;
#if 0
			msg_stddbg("p %d b %d e %d\n", precompiled_type->memobj[i]->pointer_type_idx,
					precompiled_type->memobj[i]->base_type_idx,
					precompiled_type->memobj[i]->enum_type_idx);
#endif
			assert(precompiled_type->memobj[i]->enum_type_idx + precompiled_type->memobj[i]->enum_type_steps_count ==
					precompiled_type->memobj[i]->steps_count);
		}

		/* reset counters - will use them to fill in arrays */
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
	}

	for (i = 0; i < precompiled_type->memobjs_count; i++) {
		int j;
		assert(precompiled_type->memobj[i]->steps_count == precompiled_type->memobj[i]->pointer_type_idx);
		assert(precompiled_type->memobj[i]->pointer_type_idx + precompiled_type->memobj[i]->pointer_type_steps_count == precompiled_type->memobj[i]->base_type_idx);
		assert(precompiled_type->memobj[i]->base_type_idx + precompiled_type->memobj[i]->base_type_steps_count == precompiled_type->memobj[i]->enum_type_idx);
		precompiled_type->memobj[i]->steps_count +=
				precompiled_type->memobj[i]->base_type_steps_count +
				precompiled_type->memobj[i]->enum_type_steps_count +
				precompiled_type->memobj[i]->pointer_type_steps_count;
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
	if (precompiled_type == NULL)
		return;

	if (precompiled_type->memobj != NULL ) {
		int i;
		printf("--DUMP--\n");
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
					printf("%d. value_idx %d %s %s [%d, %d) c(%p, %d) name \"%s\" anon %d ptr %d\n", j,
							step->value_index,
							step->type->name?step->type->name:(step->type->id == DW_TAG_pointer_type)?"ptr":"<no type name>",
							step->path,
							step->offset, step->byte_size,
							step->check.p_condition, step->check.expected_value,
							step->name!=NULL?step->name:"(nil)",
							step->is_anon,
							step->memobj_idx);
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
