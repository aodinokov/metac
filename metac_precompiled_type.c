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

#if 0
/*TBD: mem leaks*/
/*TODO: describe the structure*/
/*TODO: loops in types*/
/*TODO: loops in memory*/

struct metac_condition {
	int condition_index;
	int condition_value;
};

struct metac_precomiled_step { /*kind of context*/
	struct cds_list_head list;
	struct metac_type *type;
	struct metac_type *dst_type;
	char * path;
	char * dst_path;
	metac_data_member_location_t offset;
	metac_data_member_location_t dst_offset;
	int mem_obj_id;
	int dst_mem_obj_id;
	int ptr_to_mem_obj_id;
	metac_byte_size_t byte_size;
	int index;
	int parent_index;
	int conditions_count;
	struct metac_condition* conditions;
};

int metac_precomiled_step_init_path(char**p_path,
		char *path, char *name) {
	size_t path_len = path?(strlen(path)+1):0;

	(*p_path) = calloc(1, path_len + strlen(name) + 1 /*"\0"*/);
	if ((*p_path) == NULL) {
		msg_stderr("no memory\n");
		return -1;
	}

	if (path) {
		strcpy((*p_path), path);
		strcpy(&(*p_path)[path_len - 1], ".");
	}
	strcpy(&(*p_path)[path_len], name);
	return 0;
}

struct metac_precomiled_step * metac_precomiled_step_create(
		struct metac_type *type,
		int mem_obj_id,
		int index, int parent_index,
		char *path, char *name,
		metac_data_member_location_t offset, metac_byte_size_t byte_size,
		int old_conditions_count, struct metac_condition* old_conditions, struct metac_condition * new_condition) {
	struct metac_precomiled_step *res;
	if (name == NULL) {
		msg_stderr("invalid argument\n");
		return NULL;
	}
	res = calloc(1, sizeof(*res));
	if (res == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	res->dst_type = res->type = metac_type_typedef_skip(type);
	res->dst_mem_obj_id = res->mem_obj_id = mem_obj_id;
	res->ptr_to_mem_obj_id = -1;	/* needed only if it's pointer step -> show to what mem_obj_id we should go*/

	res->index = index;
	res->parent_index = parent_index;

	/* generate and save path */
	if (metac_precomiled_step_init_path(&res->path, path, name) != 0) {
		free(res);
		return NULL;
	}
	res->dst_path = res->path;

	/*save offset/byte_size*/
	res->dst_offset = res->offset = offset;
	res->byte_size = byte_size;

	/*save conditions */
	do {
		res->conditions_count = old_conditions_count + ((new_condition != NULL)?1:0);

		res->conditions = calloc(res->conditions_count, sizeof(struct metac_condition));
		if (res->conditions == NULL) {
			msg_stderr("no memory\n");
			free(res->path);
			free(res);
			return NULL;
		}
		memcpy(res->conditions, old_conditions, old_conditions_count * sizeof(struct metac_condition));
		if (new_condition != NULL) {
			memcpy(&res->conditions[old_conditions_count], new_condition, sizeof(struct metac_condition));
		}
	}while(0);

	printf("%s %s\n", res->type->name?res->type->name:(res->type->id == DW_TAG_pointer_type)?"ptr":"<no type name>", res->path);

	return res;
}

struct metac_precompiled_mem_obj {
	struct cds_list_head list;
	struct metac_type *type;
	int mem_obj_id;
};

struct metac_precompiled_mem_obj * metac_precompiled_mem_obj_create(struct metac_type *type, int mem_obj_id){
	struct metac_precompiled_mem_obj * res;
	res = calloc(1, sizeof(*res));
	if (res == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	res->type = type;
	res->mem_obj_id = mem_obj_id;

	printf("created %p\n", type);

	return res;
}

struct metac_precompiled_type {
	/* this should be done for every ptr  - if we meet ptr in table, we create metac_precompiled type for it*/
	struct metac_type *type;
	struct cds_list_head mem_obj_list;
};

/*TODO: performance can be improved - r/b tree*/
struct metac_precompiled_mem_obj * metac_precompiled_mem_obj_find(struct cds_list_head *p_mem_obj_list, struct metac_type *type) {
	struct metac_precompiled_mem_obj *entry;
	printf("looking %p\n", type);
	cds_list_for_each_entry(entry, p_mem_obj_list, list) {
		if (entry->type == type) {
			printf("found\n");
			return entry;
		}
	}
	return NULL;
}

int metac_phase1(
		struct metac_precompiled_type * precompiled_type,
		struct metac_type *base_type,	/* type for what we compile?*/
		struct metac_type *init_type,	/*mem_obj type*/
		struct cds_list_head * p_list_head) { /*go width and validate*/
	int index = 0;
	int mem_obj_count = 0;
	int conditions_count = 0;

	struct cds_list_head *current;
	struct metac_precompiled_mem_obj * mem_obj;
	mem_obj = metac_precompiled_mem_obj_create(metac_type_typedef_skip(init_type), mem_obj_count++);
	if (mem_obj == NULL)
		return -(ENOMEM);
	cds_list_add_tail(&mem_obj->list, &precompiled_type->mem_obj_list);

	struct metac_precomiled_step * step =  metac_precomiled_step_create(init_type,
			mem_obj->mem_obj_id,
			index++, -1,
			NULL, "", /*path*/
			0, metac_type_byte_size(init_type), /* offset, size */
			0, NULL, NULL /*condition*/);
	/* add first step to queue */
	cds_list_add_tail(&step->list, p_list_head);

	/* go width */
	cds_list_for_each(current, p_list_head) {
		struct metac_precomiled_step * current_step = cds_list_entry(current, struct metac_precomiled_step, list);
		struct metac_precomiled_step * next_step;
		struct metac_type *type = current_step->dst_type;	/*override this name*/

		switch(type->id) {
		case DW_TAG_base_type:
		case DW_TAG_enumeration_type:
			/* leaf types - nothing to do*/
			break;
		case DW_TAG_pointer_type: {
			metac_type_t * ptr_to_type = type->pointer_type_info.type?metac_type_typedef_skip(type->pointer_type_info.type):NULL;
			const metac_type_specification_value_t * spec = metac_type_specification(base_type, current_step->path);
			if (spec != NULL) {
				assert(spec->id == 2);
			}
			if (spec == NULL || spec->pointer_mode == pmStop || ptr_to_type == NULL)
				continue;
			//msg_stddbg("%s: spec->id %d / %d\n", current_step->path, spec->id, (int)spec->pointer_mode);

			/*allocate new mem object id */
			mem_obj = metac_precompiled_mem_obj_find(&precompiled_type->mem_obj_list, ptr_to_type);
			if (mem_obj != NULL)
				break;

			mem_obj = metac_precompiled_mem_obj_create(ptr_to_type, mem_obj_count++);
			if (mem_obj == NULL)
				return -(ENOMEM);
			cds_list_add_tail(&mem_obj->list, &precompiled_type->mem_obj_list);
			current_step->ptr_to_mem_obj_id = mem_obj->mem_obj_id;

			switch (spec->pointer_mode) {
			case pmStop:
				break;
			case pmExtendAsArray:
				next_step = metac_precomiled_step_create(ptr_to_type,
						current_step->ptr_to_mem_obj_id,
						index++, current_step->index,
						//current_step->path, "<>", /*path*/
						NULL, "", /*path*/
						0, metac_type_byte_size(ptr_to_type), /* offset, size */
						current_step->conditions_count, current_step->conditions, NULL /*condition*/);
				if (next_step == NULL)
					return -(ENOMEM);
				cds_list_add_tail(&next_step->list, p_list_head);
				break;
			case pmExtendAs1Object:
				metac_precomiled_step_init_path(&current_step->dst_path, NULL, "");
				current_step->dst_type = ptr_to_type;
				current_step->dst_mem_obj_id = current_step->ptr_to_mem_obj_id;
				current_step->dst_offset = 0;
				current = current->prev;	/*hack: make for read it one more time*/
				break;
			}
		} break;
		case DW_TAG_structure_type: {
			int anon_members_count = 0;
			metac_num_t i;
			for (i = 0; i < type->structure_type_info.members_count; i++) {
				int is_anon = 0;
				char anon_name[15];
				/*todo: check if the next type is flexible array and it's not the last element*/

				if (strcmp(type->structure_type_info.members[i].name, "") == 0) {
					is_anon = 1;
					snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
				}

				next_step = metac_precomiled_step_create(type->structure_type_info.members[i].type,
						current_step->dst_mem_obj_id,
						index++, current_step->index,
						current_step->dst_path, is_anon?anon_name:type->structure_type_info.members[i].name, /*path*/
						current_step->dst_offset + type->structure_type_info.members[i].data_member_location,
						metac_type_byte_size(type->structure_type_info.members[i].type), /*offset/byte_size*/
						current_step->conditions_count, current_step->conditions, NULL /*condition*/);
				if (next_step == NULL)
					return -(ENOMEM);
				cds_list_add_tail(&next_step->list, p_list_head);
			}
		} break;
		case DW_TAG_union_type: {
			struct metac_condition condition;
			int anon_members_count = 0;
			metac_num_t i;
			/* try to find discriminator ptr */
			const metac_type_specification_value_t * spec = metac_type_specification(base_type, current_step->path);
			if (spec == NULL) {
				msg_stddbg("Warning: Union %s doesn't have a specification - skipping its children\n", current_step->path);
				continue;
			}
			/*allocate struct to keep conditions point to fn_ptr*/
			condition.condition_index = conditions_count;
			++conditions_count;

			for (i = 0; i < type->union_type_info.members_count; i++) {
				int is_anon = 0;
				char anon_name[15];

				condition.condition_value = i;

				if (strcmp(type->structure_type_info.members[i].name, "") == 0) {
					is_anon = 1;
					snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
				}
				next_step = metac_precomiled_step_create(type->union_type_info.members[i].type,
						current_step->dst_mem_obj_id,
						index++, current_step->index,
						current_step->dst_path, is_anon?anon_name:type->union_type_info.members[i].name,
						current_step->dst_offset + type->structure_type_info.members[i].data_member_location,
						metac_type_byte_size(type->structure_type_info.members[i].type),
						current_step->conditions_count, current_step->conditions, &condition);
				if (next_step == NULL) return -(ENOMEM);
				cds_list_add_tail(&next_step->list, p_list_head);
			}
		} break;

		}
	}
	return 0;
}

metac_precompiled_type_t * metac_precompile_type(struct metac_type *type) {
	struct cds_list_head list_head;
	metac_precompiled_type_t * res = NULL;
	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return NULL;
	}
	res = calloc(1, sizeof(metac_precompiled_type_t));
	if (res == NULL) {
		msg_stderr("Can't allocate memory for metac_precompiled_type_t\n");
		return NULL;
	}
	res->type = type;
	CDS_INIT_LIST_HEAD(&res->mem_obj_list);
	/*TBD:*/
	CDS_INIT_LIST_HEAD(&list_head);
	metac_phase1(res, type, type, &list_head);

	do {
		struct metac_precomiled_step * entry;
		cds_list_for_each_entry(entry, &list_head, list) {
			int i;
			printf("%d/%d step %s %s %d->%d [%d, %d) c(%d)", entry->mem_obj_id, entry->ptr_to_mem_obj_id, entry->type->name?entry->type->name:(entry->type->id == DW_TAG_pointer_type)?"ptr":"<no type name>", entry->path,
					entry->parent_index, entry->index,
					entry->offset, entry->byte_size,
					entry->conditions_count);
			for (i = 0; i < entry->conditions_count; i++) {
				printf("(%d,%d)", entry->conditions[i].condition_index, entry->conditions[i].condition_value);
			}
			printf("\n");
		}
	}while(0);


	return res;
}

void metac_free_precompiled_type(metac_precompiled_type_t ** precompiled_type) {
	if (precompiled_type == NULL) {
		msg_stderr("invalid argument value: precompiled_type\n");
		return;
	}
	if (*precompiled_type == NULL) {
		msg_stderr("already freed\n");
		return;
	}
	/*TBD:*/
	free(*precompiled_type);
	*precompiled_type = NULL;
}

///* destruction */
struct metac_execution_context {
	/*...*/
	struct {
		void * data;
		void (*destructor_ptr)(void * data);
	}**step_data;;
};
int metac_delete(metac_precompiled_type_t * precompiled_type, void *ptr, metac_byte_size_t size);
///* C-type->some format - generic serialization*/
//int metac_unpack(metac_precompiled_type_t * precompiled_type, void *ptr, metac_byte_size_t size /*, p_dst, func and etc ToBeAdded */);

#endif /*0*/

struct condition;
struct condition_check {
	struct condition *p_condition;
	int expected_value;
};

struct step {
	struct metac_type * type;	/*type without typedef*/
	char * path;
	metac_data_member_location_t 	offset;
	metac_byte_size_t 				byte_size;

	struct step	* parent;
	struct condition_check check;

	/* if ptr */
	int 										elements_mode;			/*can be - always 1, and check fn. may be better to put spec ptr here */
	metac_array_elements_count_funtion_ptr_t	elements_count_fn_ptr;	/*if ptr or flexible array - overrides byte_size*/
	int 										memobj_idx;				/*if ptr or flrxible array*/

	/* if struct and etc - need to store children */

	int 	value_index; /*values will be stored in the array - will be initialized later*/
};

struct condition {
	struct condition_check check;
	metac_discriminator_funtion_ptr_t condition_fn_ptr;
	void * context;
};

struct memobj {
	//struct metac_type *type;

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
	struct memobj *memobj;
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

/*****************************************************************************/
int _init_path(char**p_path, char *path, char *name) {
	size_t path_len = path?(strlen(path)+1):0;

	(*p_path) = calloc(1, path_len + strlen(name) + 1 /*"\0"*/);
	if ((*p_path) == NULL) {
		msg_stderr("no memory\n");
		return -1;
	}

	if (path) {
		strcpy((*p_path), path);
		strcpy(&(*p_path)[path_len - 1], ".");
	}
	strcpy(&(*p_path)[path_len], name);
	return 0;
}

struct _step * create__step(
		int memobj_id,
		char *path, char *name,
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
	if (_init_path(&_step->p_step->path, path, name) != 0) {
		free(_step->p_step);
		free(_step);
		return NULL;
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
		}
		free(p_step);
	}
}

void delete__step(struct _step * _step) {
	if (_step) {
		delete_step(_step->p_step);
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

int _phase1(
		metac_precompiled_type_t * precompiled_type,
		struct cds_list_head * steps_list,
		struct cds_list_head * conditions_list) { /*go width, validate and generate objects */
	struct cds_list_head *pos;

	/* put first element int list*/
	struct _step * step =  create__step(
			precompiled_type->memobjs_count++,
			NULL, "", /*path*/
			precompiled_type->type,
			0, metac_type_byte_size(precompiled_type->type), /* offset, size */
			NULL);
	if (step == NULL)
		return -1;
	cds_list_add_tail(&step->list, steps_list);

	/* go width */
	cds_list_for_each(pos, steps_list) {
		struct _step * current_step = cds_list_entry(pos, struct _step, list);
		struct metac_type *type = current_step->p_step->type;
		struct _step * _next_step;

		switch(type->id) {
		case DW_TAG_base_type:
		case DW_TAG_enumeration_type:
			/* leaf types - nothing to do*/
			break;
		case DW_TAG_pointer_type: {
			metac_type_t * ptr_to_type = type->pointer_type_info.type?metac_type_typedef_skip(type->pointer_type_info.type):NULL;
			const metac_type_specification_value_t * spec = metac_type_specification(precompiled_type->type, current_step->p_step->path);
			if (spec != NULL) {
				assert(spec->id == 2);
			}
			if (spec == NULL || spec->pointer_mode == pmStop || ptr_to_type == NULL)
				continue;
			//msg_stddbg("%s: spec->id %d / %d\n", current_step->path, spec->id, (int)spec->pointer_mode);

			/*allocate new mem object id */
//			mem_obj = metac_precompiled_mem_obj_find(&precompiled_type->mem_obj_list, ptr_to_type);
//			if (mem_obj != NULL)
//				break;
//
//			mem_obj = metac_precompiled_mem_obj_create(ptr_to_type, mem_obj_count++);
//			if (mem_obj == NULL)
//				return -(ENOMEM);
//			cds_list_add_tail(&mem_obj->list, &precompiled_type->mem_obj_list);
//			current_step->ptr_to_mem_obj_id = mem_obj->mem_obj_id;
//
//			switch (spec->pointer_mode) {
//			case pmStop:
//				break;
//			case pmExtendAsArray:
//				next_step = metac_precomiled_step_create(ptr_to_type,
//						current_step->ptr_to_mem_obj_id,
//						index++, current_step->index,
//						//current_step->path, "<>", /*path*/
//						NULL, "", /*path*/
//						0, metac_type_byte_size(ptr_to_type), /* offset, size */
//						current_step->conditions_count, current_step->conditions, NULL /*condition*/);
//				if (next_step == NULL)
//					return -(ENOMEM);
//				cds_list_add_tail(&next_step->list, p_list_head);
//				break;
//			case pmExtendAs1Object:
////				metac_precomiled_step_init_path(&current_step->dst_path, NULL, "");
////				current_step->dst_type = ptr_to_type;
////				current_step->dst_mem_obj_id = current_step->ptr_to_mem_obj_id;
////				current_step->dst_offset = 0;
////				pos = pos->prev;	/*hack: make for read it one more time*/
//				break;
//			}
		} break;
		case DW_TAG_structure_type: {
			int anon_members_count = 0;
			metac_num_t i;
			for (i = 0; i < type->structure_type_info.members_count; i++) {
				int is_anon = 0;
				char anon_name[15];
				/*todo: check if the next type is flexible array and it's not the last element*/

				if (strcmp(type->structure_type_info.members[i].name, "") == 0) {
					is_anon = 1;
					snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
				}

				_next_step = create__step(
						current_step->memobj_id,
						current_step->p_step->path, is_anon?anon_name:type->structure_type_info.members[i].name, /*path*/
						type->structure_type_info.members[i].type,
						current_step->p_step->offset + type->structure_type_info.members[i].data_member_location,
						metac_type_byte_size(type->structure_type_info.members[i].type), /*offset/byte_size*/
						current_step);
				if (_next_step == NULL)
					return -(ENOMEM);
				cds_list_add_tail(&_next_step->list, steps_list);
			}
		} break;
		case DW_TAG_union_type: {
			struct _condition * _condition;
			int anon_members_count = 0;
			metac_num_t i;
			/* try to find discriminator ptr */
			const metac_type_specification_value_t * spec = metac_type_specification(precompiled_type->type, current_step->p_step->path);
			if (spec == NULL) {
				msg_stddbg("Warning: Union %s doesn't have a specification - skipping its children\n", current_step->p_step->path);
				continue;
			}

			assert(spec->id == 0);

			/*allocate struct to keep conditions point to fn_ptr*/
			_condition = create__condition(current_step->memobj_id,
					spec->discriminator_funtion_ptr,
					spec->specification_context,
					current_step);
			if (_condition == NULL)
				return -(ENOMEM);
			cds_list_add_tail(&_condition->list, conditions_list);

			for (i = 0; i < type->union_type_info.members_count; i++) {
				int is_anon = 0;
				char anon_name[15];

				if (strcmp(type->structure_type_info.members[i].name, "") == 0) {
					is_anon = 1;
					snprintf(anon_name, sizeof(anon_name), "<anon%d>", anon_members_count++);
				}
				_next_step = create__step(
						current_step->memobj_id,
						current_step->p_step->path, is_anon?anon_name:type->union_type_info.members[i].name, /*path*/
						type->union_type_info.members[i].type,
						current_step->p_step->offset + type->union_type_info.members[i].data_member_location,
						metac_type_byte_size(type->union_type_info.members[i].type), /*offset/byte_size*/
						current_step);

				/* override check for this step using new condition */
				_next_step->p_step->check.p_condition = _condition->p_condition;
				_next_step->p_step->check.expected_value = i;

				if (_next_step == NULL)
					return -(ENOMEM);
				cds_list_add_tail(&_next_step->list, steps_list);
			}
		} break;

		}
	}
	return 0;
}

void _free_phase1_lists(
		struct cds_list_head * steps_list,
		struct cds_list_head * conditions_list) {
	struct _condition * _condition, * __condition;
		struct _step * _step, * __step;

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
		struct cds_list_head * steps_list,
		struct cds_list_head * conditions_list){
	int i;
	struct _condition * _condition, * __condition;
	struct _step * _step, * __step;

	/* allocate memory for memobjs*/
	precompiled_type->memobj = calloc(precompiled_type->memobjs_count, sizeof(*precompiled_type->memobj));
	if (precompiled_type->memobj == NULL) {
		msg_stderr("Can't allocate memory\n");
		return -1;
	}

	cds_list_for_each_entry(_step, steps_list, list) {
		i = _step->memobj_id;
		assert(i < precompiled_type->memobjs_count);

		switch (_step->p_step->type->id) {
			case DW_TAG_base_type:
				++precompiled_type->memobj[i].base_type_steps_count;
				break;
			case DW_TAG_enumeration_type:
				++precompiled_type->memobj[i].enum_type_steps_count;
				break;
			case DW_TAG_pointer_type:
				++precompiled_type->memobj[i].pointer_type_steps_count;
				break;
		}
		++precompiled_type->memobj[i].steps_count;
	}
	/*get destibution of conditions over memobjs*/
	cds_list_for_each_entry(_condition, conditions_list, list) {
		i = _condition->memobj_id;
		++precompiled_type->memobj[i].conditions_count;
	}

	for (i = 0; i < precompiled_type->memobjs_count; i++) {
		/* alloc memory for conditions */
		if (precompiled_type->memobj[i].conditions_count > 0){
			precompiled_type->memobj[i].condition = calloc(precompiled_type->memobj[i].conditions_count, sizeof(*precompiled_type->memobj[i].condition));
			if (precompiled_type->memobj[i].condition == NULL) {
				return -1;
			}
		}
		/* alloc memory for steps */
		if (precompiled_type->memobj[i].steps_count > 0) {
			precompiled_type->memobj[i].step = calloc(precompiled_type->memobj[i].steps_count, sizeof(*precompiled_type->memobj[i].step));
			if (precompiled_type->memobj[i].step == NULL) {
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
			precompiled_type->memobj[i].pointer_type_idx = precompiled_type->memobj[i].steps_count -
					precompiled_type->memobj[i].pointer_type_steps_count -
					precompiled_type->memobj[i].base_type_steps_count -
					precompiled_type->memobj[i].enum_type_steps_count;
			precompiled_type->memobj[i].base_type_idx = precompiled_type->memobj[i].pointer_type_idx +
					precompiled_type->memobj[i].pointer_type_steps_count;
			precompiled_type->memobj[i].enum_type_idx = precompiled_type->memobj[i].base_type_idx +
					precompiled_type->memobj[i].base_type_steps_count;
#if 0
			msg_stddbg("p %d b %d e %d\n", precompiled_type->memobj[i].pointer_type_idx,
					precompiled_type->memobj[i].base_type_idx,
					precompiled_type->memobj[i].enum_type_idx);
#endif
			assert(precompiled_type->memobj[i].enum_type_idx + precompiled_type->memobj[i].enum_type_steps_count ==
					precompiled_type->memobj[i].steps_count);
		}

		/* reset counters - will use them to fill in arrays */
		precompiled_type->memobj[i].pointer_type_steps_count = 0;
		precompiled_type->memobj[i].base_type_steps_count = 0;
		precompiled_type->memobj[i].enum_type_steps_count = 0;
		precompiled_type->memobj[i].steps_count = 0;
		precompiled_type->memobj[i].conditions_count = 0;
	}

	cds_list_for_each_entry(_step, steps_list, list) {
		int j;
		i = _step->memobj_id;
		switch (_step->p_step->type->id) {
			case DW_TAG_base_type:
				j = precompiled_type->memobj[i].base_type_steps_count++;
				precompiled_type->memobj[i].step[precompiled_type->memobj[i].base_type_idx + j] = _step->p_step;
				break;
			case DW_TAG_enumeration_type:
				j = precompiled_type->memobj[i].enum_type_steps_count++;
				precompiled_type->memobj[i].step[precompiled_type->memobj[i].enum_type_idx + j] = _step->p_step;
				break;
			case DW_TAG_pointer_type:
				j = precompiled_type->memobj[i].pointer_type_steps_count++;
				precompiled_type->memobj[i].step[precompiled_type->memobj[i].pointer_type_idx + j] = _step->p_step;
				break;
			default:
				j = precompiled_type->memobj[i].steps_count++;
				precompiled_type->memobj[i].step[j] = _step->p_step;
		}
	}
	cds_list_for_each_entry(_condition, conditions_list, list) {
		int j;
		i = _condition->memobj_id;
		j = precompiled_type->memobj[i].conditions_count++;
		precompiled_type->memobj[i].condition[j] = _condition->p_condition;
	}

	for (i = 0; i < precompiled_type->memobjs_count; i++) {
		int j;
		assert(precompiled_type->memobj[i].steps_count == precompiled_type->memobj[i].pointer_type_idx);
		assert(precompiled_type->memobj[i].pointer_type_idx + precompiled_type->memobj[i].pointer_type_steps_count == precompiled_type->memobj[i].base_type_idx);
		assert(precompiled_type->memobj[i].base_type_idx + precompiled_type->memobj[i].base_type_steps_count == precompiled_type->memobj[i].enum_type_idx);
		precompiled_type->memobj[i].steps_count +=
				precompiled_type->memobj[i].base_type_steps_count +
				precompiled_type->memobj[i].enum_type_steps_count +
				precompiled_type->memobj[i].pointer_type_steps_count;
		for (j = 0; j < precompiled_type->memobj[i].steps_count; j++) {
			precompiled_type->memobj[i].step[j]->value_index = j;
		}
	}

	/* partially free memory */
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

	CDS_INIT_LIST_HEAD(&steps_list);
	CDS_INIT_LIST_HEAD(&conditions_list);

	if (_phase1(precompiled_type, &steps_list, &conditions_list) != 0) {
		msg_stderr("Phase 1 returned error\n");
		_free_phase1_lists(&steps_list, &conditions_list);
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

	if (_phase2(precompiled_type, &steps_list, &conditions_list) != 0) {
		msg_stderr("Phase 1 returned error\n");
		_free_phase1_lists(&steps_list, &conditions_list);
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
			if (precompiled_type->memobj[i].condition != NULL) {
				printf("Conditions:\n", i);
				for (j = 0; j < precompiled_type->memobj[i].conditions_count; j++) {
					struct condition * condition = precompiled_type->memobj[i].condition[j];
					printf("%d. addr %p fn: %p ctx: %s c(%p, %d)\n", j,
							condition,
							condition->condition_fn_ptr, (char*)condition->context,
							condition->check.p_condition, condition->check.expected_value);

				}
			}
			if (precompiled_type->memobj[i].step != NULL) {
				printf("Steps: Pointer_Idx %d Base_Idx %d Enum_Idx %d\n",
						precompiled_type->memobj[i].pointer_type_idx,
						precompiled_type->memobj[i].base_type_idx,
						precompiled_type->memobj[i].enum_type_idx);
				for (j = 0; j < precompiled_type->memobj[i].steps_count; j++) {
					struct step * step = precompiled_type->memobj[i].step[j];
					printf("%d. value_idx %d %s %s [%d, %d) c(%p, %d) ptr %d\n", j,
							step->value_index,
							step->type->name?step->type->name:(step->type->id == DW_TAG_pointer_type)?"ptr":"<no type name>",
							step->path,
							step->offset, step->byte_size,
							step->check.p_condition, step->check.expected_value,
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
			if (precompiled_type->memobj[i].condition != NULL) {
				for (j = 0; j < precompiled_type->memobj[i].conditions_count; j++) {
					delete_condition(precompiled_type->memobj[i].condition[j]);
					precompiled_type->memobj[i].condition[j] = NULL;
				}
				free(precompiled_type->memobj[i].condition);
				precompiled_type->memobj[i].condition = NULL;
			}
			if (precompiled_type->memobj[i].step != NULL) {
				for (j = 0; j < precompiled_type->memobj[i].steps_count; j++) {
					delete_step(precompiled_type->memobj[i].step[j]);
					precompiled_type->memobj[i].step[j] = NULL;
				}
				free(precompiled_type->memobj[i].step);
				precompiled_type->memobj[i].step = NULL;
			}
		}
		free(precompiled_type->memobj);
		precompiled_type->memobj = NULL;
	}

	free(*p_precompiled_type);
	*p_precompiled_type = NULL;
}
