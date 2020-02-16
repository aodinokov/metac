/*
 * metac_array_info.c
 *
 *  Created on: Feb 15, 2020
 *      Author: mralex
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>		/*uint64_t etc*/
#include <assert.h>
#include <errno.h>

#include "metac/metac_type.h"
#include "metac/metac_array_info.h"

#include "metac_oop.h"		/*_create, _delete, ...*/
#include "metac_debug.h"


metac_array_info_t * metac_array_info_create_from_type(struct metac_type *type, metac_num_t default_subrange0_count) {
	metac_num_t i;
	metac_num_t subranges_count;
	metac_array_info_t * p_array_info;
	metac_type_id_t id;

	if (type == NULL) {
		msg_stderr("invalid argument value: type\n");
		return NULL;
	}

	id = metac_type_actual_type(type)->id;
	switch (id) {
	case DW_TAG_pointer_type:
		subranges_count = 1;
		break;
	case DW_TAG_array_type:
		subranges_count = type->array_type_info.subranges_count;
		break;
	default:
		msg_stderr("metac_array_info_t can't be created from type(0x%x)\n", (int)id);
		return NULL;
	}

	p_array_info = calloc(1, sizeof(*p_array_info) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info->subranges_count = subranges_count;
	p_array_info->subranges[0].count = default_subrange0_count;

	if (id == DW_TAG_array_type) {
		for (i = 0; i < subranges_count; ++i) {
			metac_type_array_subrange_count(type, i, &p_array_info->subranges[i].count);
		}
	}

	return p_array_info;
}
metac_array_info_t * metac_array_info_create_from_elements_count(metac_count_t elements_count) {
	metac_num_t subranges_count = 1;
	metac_array_info_t * p_array_info;

	p_array_info = calloc(1, sizeof(*p_array_info) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info->subranges_count = subranges_count;
	p_array_info->subranges[0].count = elements_count;

	return p_array_info;
}
int metac_array_info_get_subrange_count(metac_array_info_t *p_array_info, metac_num_t subrange, metac_num_t *p_count) {
	if (p_array_info == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return (-EINVAL);
	}
	if (subrange >= p_array_info->subranges_count) {
		msg_stderr("invalid argument value: subrange\n");
		return (-EINVAL);
	}
	if (p_count != NULL) {
		*p_count = p_array_info->subranges[subrange].count;
	}
	return 0;
}
int metac_array_info_set_subrange_count(metac_array_info_t *p_array_info, metac_num_t subrange, metac_num_t count) {
	if (p_array_info == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return (-EINVAL);
	}
	if (subrange >= p_array_info->subranges_count) {
		msg_stderr("invalid argument value: subrange\n");
		return (-EINVAL);
	}
	p_array_info->subranges[subrange].count = count;
	return 0;
}
metac_array_info_t * metac_array_info_copy(metac_array_info_t *p_array_info_orig) {
	metac_num_t i;
	metac_num_t subranges_count;
	metac_array_info_t * p_array_info;
	metac_type_id_t id;

	if (p_array_info_orig == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return NULL;
	}

	subranges_count = p_array_info_orig->subranges_count;

	p_array_info = calloc(1, sizeof(*p_array_info) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info->subranges_count = subranges_count;

	for (i = 0; i < p_array_info->subranges_count; ++i) {
		p_array_info->subranges[i].count = p_array_info_orig->subranges[i].count;
	}

	return p_array_info;
}
int metac_array_info_delete(metac_array_info_t ** pp_array_info) {
	if (pp_array_info != NULL) {
		metac_array_info_t * p_array_info = *pp_array_info;
		if (p_array_info != NULL) {
			free(p_array_info);
		}
		*pp_array_info = NULL;
	}
	return 0;
}
metac_count_t metac_array_info_get_element_count(metac_array_info_t * p_array_info) {
	metac_num_t i;
	metac_count_t element_count;

	if (p_array_info == NULL || p_array_info->subranges_count == 0)
		return 1;

	element_count = p_array_info->subranges[0].count;
	for (i = 1; i < p_array_info->subranges_count; ++i) {
		element_count *= p_array_info->subranges[i].count;
	}
	return element_count;
}
int metac_array_info_is_equal(metac_array_info_t * p_array_info0, metac_array_info_t * p_array_info1) {
	metac_num_t i;

	if (p_array_info0 == NULL ||
		p_array_info1 == NULL) {
		return -EINVAL;
	}

	if (p_array_info0->subranges_count != p_array_info1->subranges_count)
		return 0;

	for (i = 0; i < p_array_info0->subranges_count; ++i) {
		if (p_array_info0->subranges[i].count != p_array_info1->subranges[i].count)
			return 0;
	}

	return 1;
}
metac_array_info_t * metac_array_info_create_counter(metac_array_info_t *p_array_info) {
	metac_num_t i;
	metac_num_t subranges_count;
	metac_array_info_t * p_array_info_counter;
	metac_type_id_t id;

	if (p_array_info == NULL) {
		msg_stderr("invalid argument value: p_array_info\n");
		return NULL;
	}

	subranges_count = p_array_info->subranges_count;

	p_array_info_counter = calloc(1, sizeof(*p_array_info_counter) + subranges_count * sizeof(struct _metac_array_subrange_info));
	if (p_array_info_counter == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}
	p_array_info_counter->subranges_count = subranges_count;

	for (i = 0; i < p_array_info_counter->subranges_count; ++i) {
		p_array_info_counter->subranges[i].count = 0;
	}

	return p_array_info_counter;
}

int metac_array_info_increment_counter(metac_array_info_t *p_array_info, metac_array_info_t *p_array_info_counter) {
	metac_num_t i;
	if (p_array_info == NULL ||
			p_array_info_counter == NULL ||
			p_array_info->subranges_count != p_array_info_counter->subranges_count) {
		msg_stderr("invalid argument value\n");
		return -EINVAL;
	}

	i = p_array_info_counter->subranges_count - 1;
	while(1) {
		++p_array_info_counter->subranges[i].count;
		if (p_array_info_counter->subranges[i].count == p_array_info->subranges[i].count) {
			p_array_info_counter->subranges[i].count = 0;
			if (i == 0)return 1;	/*overflow*/
			--i;
		} else break;
	};
	return 0;
}

metac_array_info_t * metac_array_info_convert_linear_id_2_subranges(
		metac_array_info_t *p_array_info,
		metac_num_t linear_id) {
	int i;

	metac_array_info_t * p_subranges_id;

	if (linear_id >= metac_array_info_get_element_count(p_array_info)) {
		msg_stderr("Can't convert linear_id %d - too big for this array_info\n", (int)linear_id);
		return NULL;
	}

	p_subranges_id = metac_array_info_copy(p_array_info);
	if (p_subranges_id == NULL) {
		return NULL;
	}

	for (i = 1; i <= p_array_info->subranges_count; ++i){
		p_subranges_id->subranges[p_array_info->subranges_count - i].count = linear_id % p_array_info->subranges[p_array_info->subranges_count - i].count;
		linear_id = linear_id / p_array_info->subranges[p_array_info->subranges_count - i].count;
	}

	assert(linear_id == 0);

	return p_subranges_id;
}
