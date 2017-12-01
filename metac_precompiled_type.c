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


