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
#include <errno.h>			/* ENOMEM etc */
#include <urcu/list.h>		/* I like struct cds_list_head :) */

/*TBD: mem leaks*/

struct metac_condition {
	int condition_index;
	int condition_value;
};

struct metac_precomiled_step { /*kind of context*/
	struct cds_list_head list;
	struct metac_type *type;
	int index;
	int parent_index;
	char * path;
	metac_data_member_location_t offset;
	metac_byte_size_t byte_size;
	int conditions_count;
	struct metac_condition* conditions;
};

struct metac_precomiled_step * metac_precomiled_struct_create(struct metac_type *type,
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

	res->type = metac_type_typedef_skip(type);

	res->index = index;
	res->parent_index = parent_index;

	/* generate and save path */
	do {
		size_t path_len = path?(strlen(path)+1):0;

		res->path = calloc(1, path_len + strlen(name) + 1 /*"\0"*/);
		if (res->path == NULL) {
			msg_stderr("no memory\n");
			free(res);
			return NULL;
		}

		if (path) {
			strcpy(res->path, path);
			strcpy(&res->path[path_len - 1], ".");
		}
		strcpy(&res->path[path_len], name);
	}while(0);

	/*save offset/byte_size*/
	res->offset = offset;
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

	/* debug */
	do {
		int i;
		printf("step %s %s %d<-%d [%d, %d) c(%d)", type->name?type->name:(type->id == DW_TAG_pointer_type)?"ptr":"<no type name>", res->path,
				res->index, res->parent_index,
				res->offset, res->byte_size,
				res->conditions_count);
		for (i = 0; i < res->conditions_count; i++) {
			printf("(%d,%d)", res->conditions[i].condition_index, res->conditions[i].condition_value);
		}
		printf("\n");
	}while(0);
	return res;
}

struct metac_precompiled_type {
	/* this should be done for every ptr  - if we meet ptr in table, we create metac_precompiled type for it*/
	struct metac_type *type;
//	metac_num_t	steps_num;
//	struct metac_precompiled_step ** steps;	/*array of pointers to steps */
};

int metac_phase1(
		struct metac_type *base_type,
		struct metac_type *init_type,
		struct cds_list_head * p_list_head) { /*go width and validate*/
	int index = 0;
	int conditions_count = 0;

	struct cds_list_head *current;
	struct metac_precomiled_step * step =  metac_precomiled_struct_create(init_type,
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
		struct metac_type *type = current_step->type;	/*override this name*/

		switch(type->id) {
		case DW_TAG_base_type:
		case DW_TAG_enumeration_type:
			/* leaf types - nothing to do*/
			break;
		case DW_TAG_pointer_type:
			/* TBD: check specifications and behave depending on it */
			break;
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

				next_step = metac_precomiled_struct_create(type->structure_type_info.members[i].type,
						index++, current_step->index,
						current_step->path, is_anon?anon_name:type->structure_type_info.members[i].name, /*path*/
						current_step->offset + type->structure_type_info.members[i].data_member_location,
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
				next_step = metac_precomiled_struct_create(type->union_type_info.members[i].type,
						index++, current_step->index,
						current_step->path, is_anon?anon_name:type->union_type_info.members[i].name,
						current_step->offset + type->structure_type_info.members[i].data_member_location,
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
	/*TBD:*/
	CDS_INIT_LIST_HEAD(&list_head);
	metac_phase1(type, type, &list_head);

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


