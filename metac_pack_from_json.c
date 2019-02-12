/*
 * metac_pack_from_json.c
 *
 *  Created on: Jan 24, 2019
 *      Author: mralex
 */

//#define METAC_DEBUG_ENABLE
#include <errno.h>			/* ENOMEM etc */
#include <assert.h>			/* assert */
#include <stdint.h>			/* int8_t - int64_t*/
#include <complex.h>		/* complext */
#include <string.h>			/* strcmp*/
#include <endian.h>			/* __BYTE_ORDER at compile time */
#include <json/json.h>		/* json_... */
#include <urcu/list.h>		/* cds_list_entry, ... */

#include "metac_type.h"
#include "metac_internals.h"		/*definitions of internal objects*/
#include "metac_debug.h"			/* msg_stderr, ...*/
#include "breadthfirst_engine.h"	/*breadthfirst_engine module*/
/*****************************************************************************/
struct runtime_context {
	struct traversing_engine traversing_engine;
	struct metac_runtime_object * runtime_object;
	json_object * p_json;

	struct cds_list_head region_list;
	struct cds_list_head executed_tasks_queue;
};

struct _region_element_element_data_ {
	metac_count_t children_count;
	json_bool found;
	json_object * p_json;
};

struct _region {
	struct cds_list_head list;
	struct region * p_region;

	/* in json */
	json_object * p_json;

	/* out json (per element, per element_element) */
	struct _region_element_element_data_ ** pp_data;
};

struct runtime_task {
	struct traversing_engine_task task;
	struct runtime_task* parent_task;

	struct _region * p__region;
};
/*****************************************************************************/
#define _COPY_TO_BITFIELDS_(_in_ptr, _out_ptr, _type_, _signed_, _mask_, _p_bit_offset, _p_bit_size) do { \
	metac_bit_offset_t shift = \
		(byte_size << 3) - ((_p_bit_size?(*_p_bit_size):0) + (_p_bit_offset?(*_p_bit_offset):0)); \
	_type_ mask = ~(((_type_)_mask_) << (_p_bit_size?(*_p_bit_size):0)); \
	_type_ _i = *((_type_*)(_in_ptr)); \
	_i = (_i & mask) << shift; \
	*(_type_*)_out_ptr ^= _i; \
}while(0)
/*****************************************************************************/
static int _metac_base_type_from_json(
		struct metac_type * type,
		json_object * p_json,
		void *ptr,
		metac_bit_offset_t * p_bit_offset,
		metac_bit_size_t * p_bit_size,
		metac_byte_size_t byte_size) {
	int count;
	const char *buf;
	int len;

	assert(type->id == DW_TAG_base_type);
	assert(json_object_get_type(p_json) == json_type_string);

	if (p_bit_offset != NULL || p_bit_size != NULL) {
		char buf[64] = {0,};
		int is_signed = type->base_type_info.encoding == DW_ATE_signed_char || type->base_type_info.encoding == DW_ATE_signed;
		if (_metac_base_type_from_json(type, p_json, buf, NULL, NULL, byte_size) != 0) {
			msg_stddbg("_metac_base_type_from_json to buf failed\n");
			return -EFAULT;
		}
		switch(type->base_type_info.byte_size){
		case sizeof(uint8_t):	_COPY_TO_BITFIELDS_(buf, ptr, uint8_t,		is_signed , 0xff, p_bit_offset, p_bit_size); break;
		case sizeof(uint16_t):	_COPY_TO_BITFIELDS_(buf, ptr, uint16_t,		is_signed , 0xffff, p_bit_offset, p_bit_size); break;
		case sizeof(uint32_t):	_COPY_TO_BITFIELDS_(buf, ptr, uint32_t,		is_signed , 0xffffffff, p_bit_offset, p_bit_size); break;
		case sizeof(uint64_t):	_COPY_TO_BITFIELDS_(buf, ptr, uint64_t,		is_signed , 0xffffffffffffffff, p_bit_offset, p_bit_size); break;
		default: msg_stderr("BITFIELDS: byte_size %d isn't supported\n", (int)type->base_type_info.byte_size); return -EINVAL;
		}
		return 0;
	}

	buf = json_object_get_string(p_json);
	len = json_object_get_string_len(p_json);

	switch(type->base_type_info.encoding) {
	case DW_ATE_unsigned_char:
	case DW_ATE_signed_char:
		msg_stddbg("DW_ATE_(un)signed_char (byte)\n");
		if (type->base_type_info.byte_size == sizeof(int8_t)) {
			if (len != 1) {
				msg_stddbg("expected single character and got: %s\n", buf);
				return -EFAULT;
			}
			char v = buf[0];
			*((char*)ptr) = v;
			return 0;
		}
	/* break removed - fallback to std approach*/
	case DW_ATE_signed:
	case DW_ATE_unsigned:
		switch (type->base_type_info.encoding) {
		case DW_ATE_signed:
		case DW_ATE_signed_char:
			msg_stddbg("DW_ATE_signed\n");
			switch (type->base_type_info.byte_size) {
			case sizeof(int8_t):  count = sscanf(buf, "%"SCNi8"" , ((int8_t* )ptr)); break;
			case sizeof(int16_t): count = sscanf(buf, "%"SCNi16"", ((int16_t*)ptr)); break;
			case sizeof(int32_t): count = sscanf(buf, "%"SCNi32"", ((int32_t*)ptr)); break;
			case sizeof(int64_t): count = sscanf(buf, "%"SCNi64"", ((int64_t*)ptr)); break;
			default: msg_stderr("DW_ATE_signed: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
			}
			break;
		case DW_ATE_unsigned:
		case DW_ATE_unsigned_char:
			msg_stddbg("DW_ATE_unsigned\n");
			switch (type->base_type_info.byte_size) {
			case sizeof(uint8_t):  count = sscanf(buf, "%"SCNu8"" , ((uint8_t* )ptr)); break;
			case sizeof(uint16_t): count = sscanf(buf, "%"SCNu16"", ((uint16_t*)ptr)); break;
			case sizeof(uint32_t): count = sscanf(buf, "%"SCNu32"", ((uint32_t*)ptr)); break;
			case sizeof(uint64_t): count = sscanf(buf, "%"SCNu64"", ((uint64_t*)ptr)); break;
			default: msg_stderr("DW_ATE_unsigned: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
			}
			break;
		}
		break;
	case DW_ATE_float:
		switch(type->base_type_info.byte_size) {
		case sizeof(float):			count = sscanf(buf, "%f",	((float*		)ptr)); break;
		case sizeof(double):		count = sscanf(buf, "%lf",	((double*		)ptr)); break;
		case sizeof(long double):	count = sscanf(buf, "%Lf",	((long double*	)ptr)); break;
		default: msg_stderr("DW_ATE_float: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
		}
		break;
	case DW_ATE_complex_float:
		switch(type->base_type_info.byte_size) {
		case sizeof(float complex): {
				float r, i;
				count = sscanf(buf, "%f + i%f", &r, &i);
				*((float complex*)ptr) = r + i * I;
			}
			break;
		case sizeof(double complex): {
				double r, i;
				count = sscanf(buf, "%lf + i%lf", &r, &i);
				*((double complex*)ptr) = r + i * I;
			}
			break;
		case sizeof(long double complex): {
				long double r, i;
				count = sscanf(buf, "%Lf + i%Lf", &r, &i);
				*((long double complex*)ptr) = r + i * I;
			}
			break;
		default: msg_stderr("DW_ATE_complex_float: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
		}
		break;
	default: msg_stderr("Unsupported encoding %d\n",(int)type->base_type_info.encoding); return -EINVAL;
	}
	if (count == 0){
		msg_stderr("Count is zero\n");
		return -EFAULT;
	}
	return 0;
}
/*****************************************************************************/
static int _metac_enumeration_type_from_json(
		struct metac_type * type,
		json_object * p_json,
		void *ptr,
		metac_byte_size_t byte_size) {
	metac_num_t i;
	metac_const_value_t const_value;
	const char* string_value;

	assert(type->id == DW_TAG_enumeration_type);
	assert(json_object_get_type(p_json) == json_type_string);

	string_value = json_object_get_string(p_json);
	for (i = 0; i < type->enumeration_type_info.enumerators_count; ++i) {
		if (strcmp(string_value, type->enumeration_type_info.enumerators[i].name) == 0) {
			const_value = type->enumeration_type_info.enumerators[i].const_value;
			break;
		}
	}
	if (!(i < type->enumeration_type_info.enumerators_count)) {
		msg_stderr("wasn't able to find enum name for %d\n", (int)const_value);
		return -EINVAL;
	}

	switch(type->enumeration_type_info.byte_size) {
	case sizeof(int8_t):	*((int8_t*	)ptr) = const_value;	break;
	case sizeof(int16_t):	*((int16_t*	)ptr) = const_value;	break;
	case sizeof(int32_t):	*((int32_t*	)ptr) = const_value;	break;
	case sizeof(int64_t):	*((int64_t*	)ptr) = const_value;	break;
	default:
#ifdef __BYTE_ORDER
		/*read maximum bits we can*/
#if __BYTE_ORDER == __LITTLE_ENDIAN
		*((int64_t*	)ptr) = const_value;	break;
#else
		*((int64_t*	)(ptr + (type->enumeration_type_info.byte_size-sizeof(int64_t)))) = const_value;	break;
#endif
#else
		/*fallback if we don't know endian*/
		msg_stderr("byte_size %d isn't supported\n", (int)type->enumeration_type_info.byte_size);
		return -EINVAL;
#endif
	}
	return 0;
}
/*****************************************************************************/
static struct _region * create__region(
		json_object * p_json,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_array_info_t *p_array_info,
		struct region * part_of_region) {
	struct _region * _region;

	_region = calloc(1, sizeof(*_region));
	if (_region == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	_region->pp_data = calloc(metac_array_info_get_element_count(p_array_info), sizeof(*_region->pp_data));
	if (_region->pp_data == NULL) {
		msg_stderr("pp_data allocation failed\n");
		free(_region);
		return NULL;
	}

	_region->p_region = create_region(NULL, byte_size, region_element_type, p_array_info, part_of_region);
	if (_region->p_region == NULL) {
		msg_stderr("create_region failed\n");
		free(_region->pp_data);
		free(_region);
		return NULL;
	}

	_region->p_json = p_json;

	return _region;
}
/*****************************************************************************/
static struct _region * simply_create__region(
		struct runtime_context * p_runtime_context,
		json_object * p_json,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_array_info_t *p_array_info,
		struct region * part_of_region) {
	/*check if region with the same addr already exists*/
	struct _region * _region = NULL;
	/*create otherwise*/
	msg_stddbg("create region_element_type for : json %s byte_size %d\n", json_object_to_json_string(p_json), (int)byte_size);
	_region = create__region(p_json, byte_size, region_element_type, p_array_info, part_of_region);
	msg_stddbg("create region_element_type result %p\n", _region);
	if (_region == NULL) {
		msg_stddbg("create__region failed\n");
		return NULL;
	}
	cds_list_add_tail(&_region->list, &p_runtime_context->region_list);
	++p_runtime_context->runtime_object->regions_count;
	return _region;
}
/*****************************************************************************/
static int delete_runtime_task(struct runtime_task ** pp_task) {
	struct runtime_task * p_task;

	if (pp_task == NULL) {
		msg_stderr("Can't delete p_task: invalid parameter\n");
		return -EINVAL;
	}

	p_task = *pp_task;
	if (p_task == NULL) {
		msg_stderr("Can't delete p_task: already deleted\n");
		return -EALREADY;
	}

	free(p_task);
	*pp_task = NULL;

	return 0;
}
/*****************************************************************************/
static struct runtime_task * create_and_add_runtime_task(
		struct traversing_engine * p_breadthfirst_engine,
		struct runtime_task * parent_task,
		traversing_engine_task_fn_t fn,
		struct _region * p__region) {
	struct runtime_task* p_task;
	struct region * p_region = p__region->p_region;

	if (p_region == NULL){
		msg_stderr("region is mandatory to create runtime_task\n");
		return NULL;
	}

	assert(p_region->elements_count > 0);

	/* allocate object */
	p_task = calloc(1, sizeof(*p_task));
	if (p_task == NULL) {
		msg_stderr("no memory\n");
		return NULL;
	}

	p_task->task.fn = fn;

	p_task->parent_task = parent_task;

	p_task->p__region = p__region;

	if (add_traversing_task_to_tail(p_breadthfirst_engine, &p_task->task) != 0) {
		msg_stderr("add_breadthfirst_task failed\n");
		free(p_task);
		return NULL;
	}
	return p_task;
}
/*****************************************************************************/
static void cleanup_runtime_context(
		struct runtime_context *p_runtime_context) {
	struct traversing_engine_task * task, *_task;
	struct _region * _region, * __region;

	cleanup_traversing_engine(&p_runtime_context->traversing_engine);

	cds_list_for_each_entry_safe(task, _task, &p_runtime_context->executed_tasks_queue, list) {
		struct runtime_task  * p_task = cds_list_entry(task, struct runtime_task, task);
		cds_list_del(&task->list);
		delete_runtime_task(&p_task);
	}

	cds_list_for_each_entry_safe(_region, __region, &p_runtime_context->region_list, list) {
		cds_list_del(&_region->list);
		if (_region->pp_data) {
			int i;
			for (i = 0; i < _region->p_region->elements_count; ++i) {
				if (_region->pp_data[i] != NULL) {
					free(_region->pp_data[i]);
					_region->pp_data[i] = NULL;
				}
			}
			free(_region->pp_data);
			_region->pp_data = NULL;
		}
		free(_region);
	}
}
/*****************************************************************************/
static int _runtime_task_fn(
		struct traversing_engine * p_breadthfirst_engine,
		struct traversing_engine_task * p_breadthfirst_engine_task,
		int error_flag) {
	int i;
	int e;
	metac_array_info_t * array_counter;
	metac_byte_size_t region_element_byte_size;
	void * region_ptr;
	metac_byte_size_t region_byte_size;
	struct runtime_context * p_context = cds_list_entry(p_breadthfirst_engine, struct runtime_context, traversing_engine);
	struct runtime_task * p_task = cds_list_entry(p_breadthfirst_engine_task, struct runtime_task, task);

	cds_list_add_tail(&p_breadthfirst_engine_task->list, &p_context->executed_tasks_queue);
	if (error_flag != 0) return 0;

	assert(p_task->p__region);
	assert(p_task->p__region->p_json);
	assert(p_task->p__region->p_region);
	assert(p_task->p__region->p_region->elements_count > 0);

	msg_stddbg("started task: region_element_type: %p (%s), json %s\n",
			p_task->p__region->p_region->elements[0].region_element_type,
			p_task->p__region->p_region->elements[0].region_element_type->type->name,
			json_object_to_json_string(p_task->p__region->p_json));

	/*support n-dimension arrays*/
	array_counter = metac_array_info_counter_init(p_task->p__region->p_region->p_array_info);
	if (array_counter == NULL){
		msg_stderr("metac_array_info_counter_init failed\n");
		return -ENOMEM;
	}

	for (e = 0; e < p_task->p__region->p_region->elements_count; e++) {
		struct region_element * p_region_element = &p_task->p__region->p_region->elements[e];
		struct _region_element_element_data_ * p_data;
		assert(p_task->p__region->p_region->elements[e].region_element_type == p_task->p__region->p_region->elements[0].region_element_type);

		/* create p_data for new element*/
		p_data = calloc(p_task->p__region->p_region->elements[0].region_element_type->members_count, sizeof(*p_data));
		if (p_data == NULL) {
			msg_stderr("can't allocate data for p_data\n");
			return -ENOMEM;
		}
		p_task->p__region->pp_data[e] = p_data;

		/*support n-dimension arrays*/
		p_data[0].p_json = json_object_array_get_idx(p_task->p__region->p_json, array_counter->subranges[0].count);
		msg_stddbg("line idx %d, getting [%d] max [%d]\n", e, array_counter->subranges[0].count, p_task->p__region->p_region->p_array_info->subranges[0].count);
		for (i = 1; i < array_counter->subranges_count; ++i) {
			p_data[0].p_json = json_object_array_get_idx(p_data[0].p_json, array_counter->subranges[i].count);
			msg_stddbg("... [%d] max [%d]\n", array_counter->subranges[i].count, p_task->p__region->p_region->p_array_info->subranges[0].count);
		}

		/* count children in _region_element_element_data_ and compare with number of fields - json_object_object_length */
		for (i = 0; i < p_region_element->region_element_type->members_count; ++i) {
			if (p_region_element->region_element_type->members[i]->parent != NULL) {
				msg_stddbg("%d, parent id %d\n", i, p_region_element->region_element_type->members[i]->parent->id);
				assert(i == p_region_element->region_element_type->members[i]->id);
				if (p_data[p_region_element->region_element_type->members[i]->parent->id].p_json == NULL)
					continue;
				//assert(p_data[p_region_element->region_element_type->element[i]->parent->id].p_json != NULL);
				if (json_object_get_type(p_data[p_region_element->region_element_type->members[i]->parent->id].p_json) != json_type_object) {
					msg_stderr("json isn't object: %s\n", json_object_to_json_string(p_data[p_region_element->region_element_type->members[i]->parent->id].p_json));
					metac_array_info_delete(&array_counter);
					return -ENOMEM;
				}
				/*TBD: this doesn't work for anonymous structs/unions: see
json:
 [ [ { "e": "0", "a": "0", "b": "0", "c": "0", "d": "0.000000" } ] ]
ERR:metac_pack_from_json.c:448:_runtime_task_fn: there are not used fields: got 1 children, json is { "e": "0", "a": "0", "b": "0", "c": "0", "d": "0.000000" }
ERR:metac_pack_from_json.c:448:_runtime_task_fn: there are not used fields: got 2 children, json is { "e": "0", "a": "0", "b": "0", "c": "0", "d": "0.000000" }
ERR:metac_pack_from_json.c:448:_runtime_task_fn: there are not used fields: got 2 children, json is { "e": "0", "a": "0", "b": "0", "c": "0", "d": "0.000000" }
ERR:metac_pack_from_json.c:448:_runtime_task_fn: there are not used fields: got 0 children, json is { "e": "0", "a": "0", "b": "0", "c": "0", "d": "0.000000" }
json_from_packed_obj:
 [ [ { "e": "0", "a": "0", "b": "0", "c": "0", "d": "0.000000" } ] ]
				 * */
				if (strlen(p_region_element->region_element_type->members[i]->name_local) > 0) {
					p_data[i].found = json_object_object_get_ex(
							p_data[p_region_element->region_element_type->members[i]->parent->id].p_json,
							p_region_element->region_element_type->members[i]->name_local,
							&p_data[i].p_json);
					++p_data[p_region_element->region_element_type->members[i]->parent->id].children_count;
				} else {
					p_data[i].p_json = p_data[p_region_element->region_element_type->members[i]->parent->id].p_json;
				}
				msg_stddbg("%d, json %p\n", i, p_data[i].p_json);
			}
		}
		/*found out how to understand what fields were not used*/
		for (i = 0; i < p_region_element->region_element_type->hierarchy_members_count; ++i) {
			int id = p_region_element->region_element_type->hierarchy_members[i]->id;
			if (p_data[id].p_json == NULL)
				continue;
			if (json_object_get_type(p_data[id].p_json) != json_type_object) {
				msg_stderr("hierarchy type got invalid json type: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				metac_array_info_delete(&array_counter);
				return -EINVAL;
			}
			if (json_object_object_length(p_data[id].p_json) > p_data[id].children_count) {
				msg_stderr("there are not used fields: got %d children, json is %s\n", p_data[id].children_count, json_object_to_json_string(p_data[id].p_json));
				/* ignore it for now
				 * metac_array_info_delete(&array_counter);
				 * return -EINVAL;
				 */
			}
		}
		/* initialize conditions */
		if (p_region_element->region_element_type->discriminators_count > 0) {
			for (i = 0; i < p_region_element->region_element_type->members_count; ++i) {
				if (p_data[p_region_element->region_element_type->members[i]->id].p_json != NULL) {
					if (set_region_element_precondition(p_region_element, &p_region_element->region_element_type->members[i]->precondition) != 0){
						msg_stderr("Element %s can't be used with others: precondition validation failed\n",
								p_region_element->region_element_type->members[i]->path_within_region_element);
					}
				}
			}
			/*check that all of them are initialized properly? how? - TBD ?*/
			/*write conditions */
			write_region_element_discriminators(p_region_element);
		}

		/*handle arrays - create simple regions (with correct ptr and size for array), initialize location immediately*/
		for (i = 0; i < p_region_element->region_element_type->array_type_elements_count; ++i) {
			int j;
			void * new_ptr;
			metac_array_info_t * p_array_info;
			metac_count_t elements_count;
			metac_byte_size_t elements_byte_size;
			struct _region * _region;
			json_object * p_json_current;
			int id = p_region_element->region_element_type->array_type_members[i]->id;
			if (p_data[id].p_json == NULL)
				continue;
			if (json_object_get_type(p_data[id].p_json) != json_type_array) {
				msg_stderr("array type got invalid json type: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				metac_array_info_delete(&array_counter);
				return -EINVAL;
			}
			assert(json_object_array_length(p_task->p__region->p_json));
			assert(p_region_element->region_element_type->array_type_members[i]->type->id == DW_TAG_array_type);

			p_array_info = metac_array_info_create_from_type(p_region_element->region_element_type->array_type_members[i]->type);
			if (p_array_info == NULL) {
				msg_stderr("metac_array_info_create failed - exiting\n");
				metac_array_info_delete(&array_counter);
				return -EFAULT;
			}
			if (p_region_element->region_element_type->array_type_members[i]->type->array_type_info.is_flexible) {
				p_array_info->subranges[0].count = json_object_array_length(p_data[id].p_json);
				/*TBD: incorrect - insert the part that implemented for non flexible*/
				/*inform that the size is the following*/
				if (p_task->p__region->p_region->elements_count > 1) {
					msg_stderr("Warning: flexible array is used within structure that is used within array\n");
				}
			}

			/*create region and add it to the task*/
			/* calculate overall elements_count */
			elements_count = metac_array_info_get_element_count(p_array_info);
			msg_stddbg("elements_count: %d\n", (int)members_count);
			elements_byte_size = p_region_element->region_element_type->array_type_members[i]->array_elements_region_element_type?
					metac_type_byte_size(p_region_element->region_element_type->array_type_members[i]->array_elements_region_element_type->type):0;

			if (elements_byte_size == 0 || elements_count == 0) {
				msg_stddbg("skipping because size is 0\n");
				metac_array_info_delete(&p_array_info);
				continue;
			}

			/*we have to create region and store it */
			_region = simply_create__region(
					p_context,
					p_data[id].p_json,
					elements_byte_size * elements_count,
					p_region_element->region_element_type->array_type_members[i]->array_elements_region_element_type,
					p_array_info,
					p_task->p__region->p_region /*TODO: probably we want to store offset*/);
			if (_region == NULL) {
				msg_stderr("Error calling find_or_create_region\n");
				metac_array_info_delete(&array_counter);
				metac_array_info_delete(&p_array_info);
				return -EFAULT;
			}

			p_region_element->p_array[i].p_region = _region->p_region;
			if (p_task->p__region->p_region->part_of_region == NULL) {
				_region->p_region->location.region_idx = p_task->p__region->p_region->unique_region_id;
				_region->p_region->location.offset =
						p_region_element->byte_size * i + p_region_element->region_element_type->array_type_members[i]->offset;
			}else{
				_region->p_region->location.region_idx = p_task->p__region->p_region->location.region_idx;
				_region->p_region->location.offset =
						p_task->p__region->p_region->location.offset +
						p_region_element->byte_size * i + p_region_element->region_element_type->array_type_members[i]->offset;
			}
			/* add task to handle this region fields properly */
			/*create the new task for this region*/
			if (create_and_add_runtime_task(p_breadthfirst_engine,
					p_task,
					_runtime_task_fn,
					_region) == NULL) {
				msg_stderr("Error calling create_and_add_runtime_task_4_region\n");
				metac_array_info_delete(&array_counter);
				return -EFAULT;
			}
		}

		/*handle pointers - if offset is 0 - create unique region and store it immediately. if offset isn't 0 - find region with the same id and offset,
		 * if no such - create like for array(handle location)*/
		for (i = 0; i < p_region_element->region_element_type->pointer_type_members_count; ++i) {
			int j;
			void * new_ptr;
			int children = 0;
			json_bool found_region_id;
			json_bool found_offset;
			struct json_object *p_json_region;
			struct json_object *p_json_region_id;
			struct json_object *p_json_offset;
			metac_array_info_t * p_array_info;
			metac_count_t elements_count;
			metac_byte_size_t elements_byte_size;
			struct _region * _region;
			json_object * p_json_current;
			int id = p_region_element->region_element_type->pointer_type_members[i]->id;

			if (p_data[id].p_json == NULL)
				continue;

			if (json_object_get_type(p_data[id].p_json) != json_type_object) {
				msg_stderr("pointer type got invalid json type: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				metac_array_info_delete(&array_counter);
				return -EINVAL;
			}

			found_region_id = json_object_object_get_ex(p_data[id].p_json, "region_id", &p_json_region_id);
			found_offset = json_object_object_get_ex(p_data[id].p_json, "offset", &p_json_offset);

			if (found_region_id)children++;
			if (found_offset)children++;
			if (json_object_object_length(p_data[id].p_json) > children) {
				msg_stderr("there are not used fields: %s\n", json_object_to_json_string(p_data[id].p_json));
				/* ignore it for now
				 * metac_array_info_delete(&array_counter);
				 * return -EINVAL;
				 */
			}
			if (!found_region_id && found_offset) {
				msg_stderr("region_id field must present if we have offset: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				metac_array_info_delete(&array_counter);
				return -EINVAL;
			}

			if (!found_region_id && !found_offset)	/*NULL-pointer*/
				continue;

			if (found_offset) {
				/*no guarantee that unique region is present at this point. we have to create all unique regions first*/
				continue;	/* pointer not to the unique region - will be handled later! TBD: implement it */
			}

			p_json_region = json_object_array_get_idx(p_context->p_json, json_object_get_int(p_json_region_id));

			p_array_info = metac_array_info_create_from_type(p_region_element->region_element_type->pointer_type_members[i]->type);
			if (p_array_info == NULL) {
				msg_stderr("metac_array_info_create failed - exiting\n");
				metac_array_info_delete(&array_counter);
				return -EFAULT;
			}
			p_array_info->subranges[0].count = json_object_array_length(p_json_region);

			if (p_region_element->region_element_type->pointer_type_members[i]->array_elements_count_funtion_ptr == NULL) {
				msg_stddbg("skipping because don't have a cb to determine elements count\n");
				metac_array_info_delete(&p_array_info);
				continue; /*we don't handle pointers if we can't get fn*/
			}

			/* calculate byte_size using length */
			elements_count = metac_array_info_get_element_count(p_array_info);
			msg_stddbg("elements_count: %d\n", (int)members_count);
			elements_byte_size = p_region_element->region_element_type->pointer_type_members[i]->array_elements_region_element_type?
					metac_type_byte_size(p_region_element->region_element_type->pointer_type_members[i]->array_elements_region_element_type->type):0;

			if (elements_byte_size == 0 || elements_count == 0) {
				msg_stddbg("skipping because size is 0\n");
				metac_array_info_delete(&p_array_info);
				continue;
			}

			if (p_context->runtime_object->unique_region[json_object_get_int(p_json_region_id)] != NULL) {
				msg_stddbg("skipping because unique region present - seems like there were concurrent pointers\n");
				metac_array_info_delete(&p_array_info);
				continue;
			}

			/*we have to create region and store it (need create or find to support loops) */
			_region = simply_create__region(
					p_context,
					p_json_region,
					elements_byte_size * elements_count,
					p_region_element->region_element_type->pointer_type_members[i]->array_elements_region_element_type,
					p_array_info,
					NULL);
			if (_region == NULL) {
				msg_stderr("Error calling find_or_create_region\n");
				metac_array_info_delete(&p_array_info);
				metac_array_info_delete(&array_counter);
				return -EFAULT;
			}
			p_region_element->p_pointer[i].p_region = _region->p_region;
			_region->p_region->unique_region_id = json_object_get_int(p_json_region_id);
			p_context->runtime_object->unique_region[_region->p_region->unique_region_id] = _region->p_region;


			/* add task to handle this region fields properly */
			if (create_and_add_runtime_task(p_breadthfirst_engine,
					p_task,
					_runtime_task_fn,
					_region) == NULL) {
				msg_stderr("Error calling create_and_add_runtime_task_4_region\n");
				metac_array_info_delete(&array_counter);
				return -EFAULT;
			}
		}
		metac_array_info_counter_increment(p_task->p__region->p_region->p_array_info, array_counter);
	}

	metac_array_info_delete(&array_counter);
	msg_stddbg("finished task\n");
	return 0;
}
/*****************************************************************************/
static struct metac_runtime_object * create_runtime_object_from_json(
		struct metac_precompiled_type * p_precompiled_type,
		json_object * p_json) {
	int i, j, k;
	metac_array_info_t * p_array_info;
	struct runtime_context context;
	struct region * region;
	struct _region * _region;
	json_object * p_json_unique_region;

	context.runtime_object = create_runtime_object(p_precompiled_type);
	if (context.runtime_object == NULL) {
		msg_stderr("create_runtime_object failed\n");
		return NULL;
	}
	CDS_INIT_LIST_HEAD(&context.region_list);
	CDS_INIT_LIST_HEAD(&context.executed_tasks_queue);

	/*use breadthfirst_engine*/
	if (init_traversing_engine(&context.traversing_engine) != 0){
		msg_stderr("create_breadthfirst_engine failed\n");
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	context.p_json = p_json;
	context.runtime_object->unique_regions_count = json_object_array_length(p_json);
	assert(context.runtime_object->unique_regions_count > 0);

	context.runtime_object->unique_region = calloc(
			context.runtime_object->unique_regions_count, sizeof(*context.runtime_object->unique_region));
	if (context.runtime_object->unique_region == NULL) {
		msg_stderr("create_runtime_object failed\n");
		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	p_json_unique_region = json_object_array_get_idx(p_json, 0);
	if (json_object_get_type(p_json_unique_region) != json_type_array) {
		msg_stderr("p_json_unique_region isn't array\n");
		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	p_array_info = metac_array_info_create_from_elements_count(json_object_array_length(p_json_unique_region));
	if (p_array_info == NULL){
		msg_stderr("metac_array_info_create_from_elements_count failed\n");
		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	_region = simply_create__region(&context,
			p_json_unique_region,
			metac_type_byte_size(p_precompiled_type->region_element_type[0]->type) * metac_array_info_get_element_count(p_array_info),
			p_precompiled_type->region_element_type[0],
			p_array_info,
			NULL);
	if (_region == NULL) {
		msg_stderr("create_region failed\n");
		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}
	_region->p_region->unique_region_id = 0;
	context.runtime_object->unique_region[_region->p_region->unique_region_id] = _region->p_region;

	/*add task for the first region*/
	if (create_and_add_runtime_task(&context.traversing_engine,
			NULL,
			_runtime_task_fn,
			_region) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);	/*region_array isn't initialized yet, runtime_object won't take care of regions*/
		}

		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}
	if (run_traversing_engine(&context.traversing_engine) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	context.runtime_object->region = calloc(context.runtime_object->regions_count, sizeof(*(context.runtime_object->region)));
	if (context.runtime_object->region == NULL) {
		msg_stderr("Can't allocate memory for regions array in runtime_object\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	i = 0;
	cds_list_for_each_entry(_region, &context.region_list, list) {
		_region->p_region->id = i;
		context.runtime_object->region[i] = _region->p_region;
		++i;
	}
	assert(context.runtime_object->regions_count == i);

	/*so, we have object, but regions don't have memory allocated, let's allocate it*/
	for (i = 0; i < context.runtime_object->unique_regions_count; ++i) {
		/*TBD: check that all unique_regions are not NULL*/
		void *ptr = calloc(context.runtime_object->unique_region[i]->byte_size, 1);
		if (ptr == NULL) {
			msg_stderr("can't allocate unique region %d size\n", context.runtime_object->unique_region[i]->byte_size);
			cleanup_runtime_context(&context);
			free_runtime_object(&context.runtime_object);
			return NULL;
		}
		msg_stddbg("unique_region %d, updating ptr to %p, size is %d\n",
				i, ptr, context.runtime_object->unique_region[i]->byte_size);
		update_region_ptr(context.runtime_object->unique_region[i], ptr);
	}

	/*now go though all pointers and finish pointers to non-unique regions*/
	cds_list_for_each_entry(_region, &context.region_list, list) {
		/*update ptr for non-unique region*/
		if (_region->p_region->part_of_region != NULL) {
			assert(_region->p_region->location.region_idx < context.runtime_object->unique_regions_count);
			assert(_region->p_region->location.offset < context.runtime_object->unique_region[_region->p_region->location.region_idx]->byte_size);
			update_region_ptr(_region->p_region,
					context.runtime_object->unique_region[_region->p_region->location.region_idx]->ptr + _region->p_region->location.offset);
			msg_stddbg("region %d, updated ptr to %p (unique region %d offset %d), size is %d\n",
					_region->p_region->id,
					_region->p_region->ptr,
					_region->p_region->location.region_idx,
					_region->p_region->location.offset,
					_region->p_region->byte_size);
		}
		/*set pointers*/
		for (j = 0; j < _region->p_region->elements_count; ++j) {
			for (k = 0; k < _region->p_region->elements[j].region_element_type->pointer_type_members_count; ++k) {
				int id = _region->p_region->elements[j].region_element_type->pointer_type_members[k]->id;
				if (_region->pp_data[j] != NULL &&
					_region->pp_data[j][id].p_json != NULL) {
					void * ptr_val;

					metac_count_t region_id;
					metac_data_member_location_t offset;

					struct json_object *p_json_region_id;
					struct json_object *p_json_offset;
					json_bool found_region_id = json_object_object_get_ex(_region->pp_data[j][id].p_json, "region_id", &p_json_region_id);
					json_bool found_offset = json_object_object_get_ex(_region->pp_data[j][id].p_json, "offset", &p_json_offset);
					if (!found_region_id)
						continue;

					region_id = json_object_get_int(p_json_region_id);
					ptr_val = context.runtime_object->unique_region[region_id]->ptr;

					if (found_offset){
						offset = json_object_get_int(p_json_offset);
						ptr_val += offset;
					}
					/*set ptr*/
					assert(_region->p_region->elements[j].ptr);
					assert(_region->p_region->elements[j].region_element_type->pointer_type_members[k]->offset < _region->p_region->elements[j].byte_size);

					msg_stddbg("pointer at %p set to %p\n",
							_region->p_region->elements[j].ptr + _region->p_region->elements[j].region_element_type->pointer_type_members[k]->offset,
							ptr_val);

					*((void**)(_region->p_region->elements[j].ptr + _region->p_region->elements[j].region_element_type->pointer_type_members[k]->offset)) = ptr_val;
				}
			}
			/*handle base - TODO:make a warning if we're writing to non 0 and value isn't the same*/
			for (k = 0; k < _region->p_region->elements[j].region_element_type->base_type_members_count; ++k) {
				int id = _region->p_region->elements[j].region_element_type->base_type_members[k]->id;
				if (_region->pp_data[j] != NULL &&
					_region->pp_data[j][id].p_json != NULL) {
					if (json_object_get_type(_region->pp_data[j][id].p_json) != json_type_string) {
						msg_stderr("base type got invalid json type: %s\n",
								json_object_to_json_string(_region->pp_data[j][id].p_json));
						cleanup_runtime_context(&context);
						free_runtime_object(&context.runtime_object);
						return NULL;
					}
					/*pack base from json*/
					if (_metac_base_type_from_json(
							_region->p_region->elements[j].region_element_type->base_type_members[k]->type,
							_region->pp_data[j][id].p_json,
							_region->p_region->elements[j].ptr + _region->p_region->elements[j].region_element_type->base_type_members[k]->offset,
							_region->p_region->elements[j].region_element_type->base_type_members[k]->p_bit_offset,
							_region->p_region->elements[j].region_element_type->base_type_members[k]->p_bit_size,
							_region->p_region->elements[j].region_element_type->base_type_members[k]->byte_size ) != 0) {
						msg_stderr("_metac_base_type_from_json failed for : %s\n",
								json_object_to_json_string(_region->pp_data[j][id].p_json));
						cleanup_runtime_context(&context);
						free_runtime_object(&context.runtime_object);
						return NULL;
					}
				}
			}
			/*handle enums - TODO:make a warning if we're writing to non 0 and value isn't the same*/
			for (k = 0; k < _region->p_region->elements[j].region_element_type->enum_type_members_count; ++k) {
				int id = _region->p_region->elements[j].region_element_type->enum_type_members[k]->id;
				if (_region->pp_data[j] != NULL &&
					_region->pp_data[j][id].p_json != NULL) {
					if (json_object_get_type(_region->pp_data[j][id].p_json) != json_type_string) {
						msg_stderr("enum type got invalid json type: %s\n",
								json_object_to_json_string(_region->pp_data[j][id].p_json));
						cleanup_runtime_context(&context);
						free_runtime_object(&context.runtime_object);
						return NULL;
					}
					/*pack enum from json*/
					if (_metac_enumeration_type_from_json(
							_region->p_region->elements[j].region_element_type->type,
							_region->pp_data[j][id].p_json,
							_region->p_region->elements[j].ptr +  _region->p_region->elements[j].region_element_type->enum_type_members[k]->offset,
							_region->p_region->elements[j].region_element_type->enum_type_members[k]->byte_size ) != 0) {
						msg_stderr("_metac_enumeration_type_from_json failed for : %s\n",
								json_object_to_json_string(_region->pp_data[j][id].p_json));
						cleanup_runtime_context(&context);
						free_runtime_object(&context.runtime_object);
						return NULL;
					}
				}
			}
			++i;
		}
	}
	cleanup_runtime_context(&context);
	msg_stddbg("obj %p\n", context.runtime_object);
	return context.runtime_object;
}
/*****************************************************************************/
int metac_pack_from_json(
		metac_precompiled_type_t * precompiled_type,
		json_object * p_json,
		void **p_ptr,
		metac_byte_size_t * p_size,
		metac_count_t * p_elements_count
		) {
	metac_count_t unique_regions_count;
	struct metac_runtime_object * p_runtime_object;

	/*check consistency first*/
	if (json_object_get_type(p_json) != json_type_array) {
		msg_stderr("p_json isn't array\n");
		return -EINVAL;
	}
	unique_regions_count = json_object_array_length(p_json);
	if (unique_regions_count == 0) {
		msg_stderr("p_json has zero length\n");
		return -EINVAL;
	}
	p_runtime_object = create_runtime_object_from_json(precompiled_type, p_json);
	if (p_runtime_object == NULL) {
		msg_stderr("create_runtime_object_from_json returned error\n");
		return -EFAULT;
	}

	if (p_ptr) {
		*p_ptr = p_runtime_object->unique_region[0]->ptr;
	}
	if (p_size) {
		*p_size = p_runtime_object->unique_region[0]->byte_size;
	}
	if (p_elements_count) {
		*p_elements_count = p_runtime_object->unique_region[0]->elements_count;
	}

	free_runtime_object(&p_runtime_object);
	return 0;
}
