/*
 * metac_pack_from_json.c
 *
 *  Created on: Jan 24, 2019
 *      Author: mralex
 */

#define METAC_DEBUG_ENABLE
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
	struct metac_runtime_object * runtime_object;
	json_object * p_json;

	struct cds_list_head region_list;
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
	struct breadthfirst_engine_task task;
	struct runtime_task* parent_task;

	struct _region * p__region;
};
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

	buf = json_object_get_string(p_json);
	len = json_object_get_string_len(p_json);

//	if (p_bit_offset != NULL || p_bit_size != NULL) {
//		int is_signed = type->base_type_info.encoding == DW_ATE_signed_char || type->base_type_info.encoding == DW_ATE_signed;
//		switch(type->base_type_info.byte_size){
//		case sizeof(uint8_t):	_COPY_FROM_BITFIELDS_(buf, uint8_t,		is_signed , 0xff, p_bit_offset, p_bit_size); break;
//		case sizeof(uint16_t):	_COPY_FROM_BITFIELDS_(buf, uint16_t,	is_signed , 0xffff, p_bit_offset, p_bit_size); break;
//		case sizeof(uint32_t):	_COPY_FROM_BITFIELDS_(buf, uint32_t,	is_signed , 0xffffffff, p_bit_offset, p_bit_size); break;
//		case sizeof(uint64_t):	_COPY_FROM_BITFIELDS_(buf, uint64_t,	is_signed , 0xffffffffffffffff, p_bit_offset, p_bit_size); break;
//		default: msg_stderr("BITFIELDS: byte_size %d isn't supported\n", (int)type->base_type_info.byte_size); return -EINVAL;
//		}
//		return _metac_base_type_to_json(type, buf, NULL, NULL, byte_size, pp_json);
//	}

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
			break;
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
//				float complex v = *((float complex*)ptr);
//				count = snprintf(buf, "%f + i%f", creal(v), cimag(v));
			}
			break;
		case sizeof(double complex): {
//				double complex v = *((double complex*)ptr);
//				count = snprintf(buf, "%f + i%f", creal(v), cimag(v));
			}
			break;
		case sizeof(long double complex): {
//				long double complex v = *((long double complex*)ptr);
//				count = snprintf(buf, "%f + i%f", creal(v), cimag(v));
			}
			break;
		default: msg_stderr("DW_ATE_complex_float: Unsupported byte_size %d\n",(int)type->base_type_info.byte_size); return -EINVAL;
		}
		break;
	default: msg_stderr("Unsupported encoding %d\n",(int)type->base_type_info.encoding); return -EINVAL;
	}

//	if (pp_json) {
//		*pp_json = json_object_new_string_len(buf, len);
//	}
	return 0;
}
/*****************************************************************************/
static int _metac_enumeration_type_from_json(
		struct metac_type * type,
		json_object * p_json,
		void *ptr,
		metac_byte_size_t byte_size) {
//	metac_num_t i;
//	metac_const_value_t const_value;
//
//	assert(type->id == DW_TAG_enumeration_type);
//	switch(type->enumeration_type_info.byte_size) {
//	case sizeof(int8_t):	const_value = *((int8_t*	)ptr);	break;
//	case sizeof(int16_t):	const_value = *((int16_t*	)ptr);	break;
//	case sizeof(int32_t):	const_value = *((int32_t*	)ptr);	break;
//	case sizeof(int64_t):	const_value = *((int64_t*	)ptr);	break;
//	default:
//#ifdef __BYTE_ORDER
//		/*read maximum bits we can*/
//#if __BYTE_ORDER == __LITTLE_ENDIAN
//		const_value = *((int64_t*	)ptr);	break;
//#else
//		const_value = *((int64_t*	)(ptr + (type->enumeration_type_info.byte_size-sizeof(int64_t))));	break;
//#endif
//#else
//		/*fallback if we don't know endian*/
//		msg_stderr("byte_size %d isn't supported\n", (int)type->enumeration_type_info.byte_size);
//		return -EINVAL;
//#endif
//	}
//
//	for (i = 0; i < type->enumeration_type_info.enumerators_count; ++i) {
//		if (const_value == type->enumeration_type_info.enumerators[i].const_value) {
//			if (pp_json) {
//				*pp_json = json_object_new_string(type->enumeration_type_info.enumerators[i].name);
//			}
//			return 0;
//		}
//	}
//	msg_stderr("wasn't able to find enum name for %d\n", (int)const_value);
	//return -EINVAL;
	return 0;
}
/*****************************************************************************/
static struct _region * create__region(
		void *ptr,
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

	_region->p_region = create_region(ptr, byte_size, region_element_type, p_array_info, part_of_region);
	if (_region->p_region == NULL) {
		msg_stderr("create_region failed\n");
		free(_region);
		return NULL;
	}

	return _region;
}
/*****************************************************************************/
static struct _region * simply_create__region(
		struct runtime_context * p_runtime_context,
		void *ptr,
		metac_byte_size_t byte_size,
		struct region_element_type * region_element_type,
		metac_array_info_t *p_array_info,
		struct region * part_of_region) {
	/*check if region with the same addr already exists*/
	struct _region * _region = NULL;
	/*create otherwise*/
	msg_stddbg("create region_element_type for : ptr %p byte_size %d\n", ptr, (int)byte_size);
	_region = create__region(ptr, byte_size, region_element_type, p_array_info, part_of_region);
	msg_stddbg("create region_element_type result %p\n", _region);
	if (_region == NULL) {
		msg_stddbg("create__region failed\n");
		return NULL;
	}
	cds_list_add_tail(&_region->list, &p_runtime_context->region_list);
	++p_runtime_context->runtime_object->regions_count;
	return _region;
}

//static struct _region * find_or_create__region(
//		struct runtime_context * p_runtime_context,
//		void *ptr,
//		metac_byte_size_t byte_size,
//		struct region_element_type * region_element_type,
//		metac_count_t elements_count,
//		struct region * part_of_region,
//		int * p_created_flag) {
//	/*check if region with the same addr already exists*/
//	struct _region * _region = NULL;
//	struct _region * _region_iter;
//
//	if (p_created_flag != NULL) *p_created_flag = 0;
//
//	cds_list_for_each_entry(_region_iter, &p_runtime_context->region_list, list) {
//		if (ptr == _region_iter->p_region->ptr &&
//				byte_size == _region_iter->p_region->byte_size) { /* case when ptr is inside will be covered later */
//			_region = _region_iter;
//			msg_stddbg("found region %p\n", _region);
//			break;
//		}
//	}
//
//	if (_region == NULL) {
//		_region = simply_create__region(p_runtime_context, ptr, byte_size, region_element_type, elements_count, part_of_region);
//		if (_region != NULL) {
//			if (p_created_flag != NULL) *p_created_flag = 1;
//		}
//	}
//
//	return _region;
//}
/*****************************************************************************/
static void cleanup_runtime_context(
		struct runtime_context *p_runtime_context) {
	struct _region * _region, * __region;

	cds_list_for_each_entry_safe(_region, __region, &p_runtime_context->region_list, list) {
		cds_list_del(&_region->list);
		free(_region);
	}
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
		struct breadthfirst_engine * p_breadthfirst_engine,
		struct runtime_task * parent_task,
		breadthfirst_engine_task_fn_t fn,
		breadthfirst_engine_task_destructor_t destroy,
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
	p_task->task.destroy = destroy;

	p_task->parent_task = parent_task;

	p_task->p__region = p__region;

	if (add_breadthfirst_task(p_breadthfirst_engine, &p_task->task) != 0) {
		msg_stderr("add_breadthfirst_task failed\n");
		free(p_task);
		return NULL;
	}
	return p_task;
}
/*****************************************************************************/
static int _runtime_task_destroy_fn(
	struct breadthfirst_engine * p_breadthfirst_engine,
	struct breadthfirst_engine_task * p_breadthfirst_engine_task) {
	struct runtime_task * p_task = cds_list_entry(p_breadthfirst_engine_task, struct runtime_task, task);
	delete_runtime_task(&p_task);
	return 0;
}
/*****************************************************************************/
static int _runtime_task_fn(
		struct breadthfirst_engine * p_breadthfirst_engine,
		struct breadthfirst_engine_task * p_breadthfirst_engine_task) {
	int i;
	int e;
	metac_byte_size_t region_element_byte_size;
	void * region_ptr;
	metac_byte_size_t region_byte_size;
	struct runtime_task * p_task = cds_list_entry(p_breadthfirst_engine_task, struct runtime_task, task);
	struct runtime_context * p_context = (struct runtime_context *)p_breadthfirst_engine->private_data;
	struct _region_element_element_data_ * p_data;

	assert(p_task->p__region);
	assert(p_task->p__region->p_json);
	assert(p_task->p__region->p_region);
	assert(p_task->p__region->p_region->elements_count > 0);

	msg_stddbg("started task: region_element_type: %p (%s), json %s\n",
			p_task->p__region->p_region->elements[0].region_element_type,
			p_task->p__region->p_region->elements[0].region_element_type->type->name,
			json_object_to_json_string(p_task->p__region->p_json));

	/*TODO: store it to _region per element*/
	p_data = calloc(p_task->p__region->p_region->elements[0].region_element_type->elements_count, sizeof(*p_data));
	if (p_data == NULL) {
		msg_stderr("can't allocate data for p_data\n");
		return -ENOMEM;
	}

	/*calculate size*/
	region_element_byte_size = metac_type_byte_size(p_task->p__region->p_region->elements[0].region_element_type->type);
	/*cover case when we have 1 element and flexible array - we have to increase region size*/
	//TBD:if (p_task->p__region->p_region->elements[0].region_element_type->is_flexible)

	/*allocate mem and reinitialize region and elements*/
	if (p_task->p__region->p_region->ptr == NULL) {
		region_byte_size = region_element_byte_size * p_task->p__region->p_region->elements_count;
		region_ptr = calloc(p_task->p__region->p_region->elements_count, region_element_byte_size);
		if (region_ptr == NULL) {
			msg_stderr("can't allocate data for p_data\n");
			free(p_data);
			return -ENOMEM;
		}
		if (update_region_ptr_and_size(p_task->p__region->p_region,
				region_ptr,
				region_byte_size) != 0) {
			msg_stderr("update_region_ptr_and_size failed\n");
			free(p_data);
			return -ENOMEM;
		}
	}

	/*todo: support n-dimension arrays!*/
	assert(p_task->p__region->p_region->elements_count == json_object_array_length(p_task->p__region->p_json));
	for (e = 0; e < p_task->p__region->p_region->elements_count; e++) {
		struct region_element * p_region_element = &p_task->p__region->p_region->elements[e];

		assert(p_task->p__region->p_region->elements[e].region_element_type == p_task->p__region->p_region->elements[0].region_element_type);

		/*reset p_data for new element*/
		memset(p_data, 0, p_region_element->region_element_type->elements_count * sizeof(*p_data));
		/*todo: support n-dimension arrays!*/
		p_data[0].p_json = json_object_array_get_idx(p_task->p__region->p_json, e);

		/*TODO: found out how to understand what fields were not used -
		 * count children in _region_element_element_data_ and compare with number of fields - json_object_object_length
		 */
		for (i = 0; i < p_region_element->region_element_type->elements_count; ++i) {
			if (p_region_element->region_element_type->element[i]->parent != NULL) {
				assert(p_data[p_region_element->region_element_type->element[i]->parent->id].p_json != NULL);
				assert(i == p_region_element->region_element_type->element[i]->id);
				if (json_object_get_type(p_data[p_region_element->region_element_type->element[i]->parent->id].p_json) != json_type_object) {
					msg_stderr("json isn't object: %s\n", json_object_to_json_string(p_data[p_region_element->region_element_type->element[i]->parent->id].p_json));
					free(p_data);
					return -ENOMEM;
				}
				if (strlen(p_region_element->region_element_type->element[i]->name_local) > 0) {
					p_data[i].found = json_object_object_get_ex(
							p_data[p_region_element->region_element_type->element[i]->parent->id].p_json,
							p_region_element->region_element_type->element[i]->name_local,
							&p_data[i].p_json);
					++p_data[p_region_element->region_element_type->element[i]->parent->id].children_count;
				} else {
					p_data[i].p_json = p_data[p_region_element->region_element_type->element[i]->parent->id].p_json;
				}
			}
		}
		/*found out how to understand what fields were not used*/
		for (i = 0; i < p_region_element->region_element_type->hierarchy_elements_count; ++i) {
			int id = p_region_element->region_element_type->hierarchy_element[i]->id;
			if (json_object_get_type(p_data[id].p_json) != json_type_object) {
				msg_stderr("hierarchy type got invalid json type: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				free(p_data);
				return -EINVAL;
			}
			if (json_object_object_length(p_data[id].p_json) > p_data[id].children_count) {
				msg_stderr("there are not used fields: %s\n", json_object_to_json_string(p_data[id].p_json));
				/* ignore it for now
				 * free(p_data);
				 * return -EINVAL;
				 */
			}
		}
		/* initialize conditions */
		if (p_region_element->region_element_type->discriminators_count > 0) {
			for (i = 0; i < p_region_element->region_element_type->elements_count; ++i) {
				if (p_data[p_region_element->region_element_type->element[i]->id].p_json != NULL) {
					if (set_region_element_precondition(p_region_element, &p_region_element->region_element_type->element[i]->precondition) != 0){
						msg_stderr("Element %s can't be used with others: precondition validation failed\n",
								p_region_element->region_element_type->element[i]->path_within_region_element);
					}
				}
			}
			/*check that all of them are initialized properly? how? - TBD ?*/
			/*write conditions */
			write_region_element_discriminators(p_region_element);
		}
		/*handle base - TODO:make a warning if we're writing to non 0 and value isn't the same*/
		for (i = 0; i < p_region_element->region_element_type->base_type_elements_count; ++i) {
			int id = p_region_element->region_element_type->base_type_element[i]->id;
			if (json_object_get_type(p_data[id].p_json) != json_type_string) {
				msg_stderr("base type got invalid json type: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				free(p_data);
				return -EINVAL;
			}
			/*pack base from json*/
			if (_metac_base_type_from_json(
					p_region_element->region_element_type->base_type_element[i]->type,
					p_data[id].p_json,
					p_region_element->ptr + p_region_element->region_element_type->base_type_element[i]->offset,
					p_region_element->region_element_type->base_type_element[i]->p_bit_offset,
					p_region_element->region_element_type->base_type_element[i]->p_bit_size,
					p_region_element->region_element_type->base_type_element[i]->byte_size ) != 0) {
				msg_stderr("_metac_base_type_from_json failed for : %s\n",
						json_object_to_json_string(p_data[id].p_json));
				free(p_data);
				return -EINVAL;
			}
		}
		/*handle enums - TODO:make a warning if we're writing to non 0 and value isn't the same*/
		for (i = 0; i < p_region_element->region_element_type->enum_type_elements_count; ++i) {
			int id = p_region_element->region_element_type->enum_type_element[i]->id;
			if (json_object_get_type(p_data[id].p_json) != json_type_string) {
				msg_stderr("enum type got invalid json type: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				free(p_data);
				return -EINVAL;
			}
			/*pack enum from json*/
			if (_metac_enumeration_type_from_json(
					p_region_element->region_element_type->base_type_element[i]->type,
					p_data[id].p_json,
					p_region_element->ptr + p_region_element->region_element_type->base_type_element[i]->offset,
					p_region_element->region_element_type->base_type_element[i]->byte_size ) != 0) {
				msg_stderr("_metac_enumeration_type_from_json failed for : %s\n",
						json_object_to_json_string(p_data[id].p_json));
				free(p_data);
				return -EINVAL;
			}
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
			int id = p_region_element->region_element_type->array_type_element[i]->id;
			if (json_object_get_type(p_data[id].p_json) != json_type_array) {
				msg_stderr("array type got invalid json type: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				free(p_data);
				return -EINVAL;
			}
			assert(json_object_array_length(p_task->p__region->p_json));
			assert(p_region_element->region_element_type->array_type_element[i]->type->id == DW_TAG_array_type);

			p_array_info = metac_array_info_create_from_type(p_region_element->region_element_type->array_type_element[i]->type);
			if (p_array_info == NULL) {
				msg_stderr("metac_array_info_create failed - exiting\n");
				return -EFAULT;
			}
			if (p_region_element->region_element_type->array_type_element[i]->type->array_type_info.is_flexible) {
				p_array_info->subranges[0].count = json_object_array_length(p_data[id].p_json);
			}

			/* set ptr to the first element */
			new_ptr = (void*)(p_region_element->ptr + p_region_element->region_element_type->array_type_element[i]->offset);

			/*work with array size(s)*/
			if (p_region_element->region_element_type->array_type_element[i]->type->array_type_info.is_flexible != 0) {
				/*TBD: incorrect - insert the part that implemented for non flexible*/
				/*inform that the size is the following*/
				if (p_task->p__region->p_region->elements_count > 0) {
					msg_stderr("Warning: flexible array is used within structure that is used within array\n");
				}
				if (p_region_element->region_element_type->array_type_element[i]->array_elements_count_funtion_ptr == NULL) {
					msg_stddbg("Warning: can't save flexible array size because don't have a cb to determine elements count\n");
				}
				if (p_region_element->region_element_type->array_type_element[i]->array_elements_count_funtion_ptr != NULL) {
					if (p_region_element->region_element_type->array_type_element[i]->array_elements_count_funtion_ptr(
							1,
							p_region_element->ptr,
							p_region_element->region_element_type->type,
							new_ptr,
							p_region_element->region_element_type->array_type_element[i]->array_elements_region_element_type?
									p_region_element->region_element_type->array_type_element[i]->array_elements_region_element_type->type:NULL,
							p_array_info,
							p_region_element->region_element_type->array_type_element[i]->array_elements_count_cb_context) != 0) {
						msg_stderr("Error calling array_elements_count_funtion_ptr for pointer element %d in type %s\n",
								i, p_region_element->region_element_type->type->name);
						free(p_data);
						metac_array_info_delete(&p_array_info);
						return -EFAULT;
					}
				}
			}
			/*create region and add it to the task*/
			/* calculate overall elements_count */
			elements_count = metac_array_info_get_element_count(p_array_info);
			msg_stddbg("elements_count: %d\n", (int)elements_count);
			elements_byte_size = p_region_element->region_element_type->array_type_element[i]->array_elements_region_element_type?
					metac_type_byte_size(p_region_element->region_element_type->array_type_element[i]->array_elements_region_element_type->type):0;

			if (elements_byte_size == 0 || elements_count == 0) {
				msg_stddbg("skipping because size is 0\n");
				metac_array_info_delete(&p_array_info);
				continue;
			}

			/*we have to create region and store it */
			_region = simply_create__region(
					p_context,
					new_ptr,
					elements_byte_size * elements_count,
					p_region_element->region_element_type->array_type_element[i]->array_elements_region_element_type,
					p_array_info,
					/*TBD: add n-1 dimension info,*/
					p_task->p__region->p_region);
			if (_region == NULL) {
				msg_stderr("Error calling find_or_create_region\n");
				free(p_data);
				metac_array_info_delete(&p_array_info);
				return -EFAULT;
			}
			_region->p_json = p_data[id].p_json;

			p_region_element->p_array[i].p_region = _region->p_region;
			/* add task to handle this region fields properly */
			/*create the new task for this region*/
			if (create_and_add_runtime_task(p_breadthfirst_engine,
					p_task,
					_runtime_task_fn,
					_runtime_task_destroy_fn, _region) == NULL) {
				msg_stderr("Error calling create_and_add_runtime_task_4_region\n");
				free(p_data);
				return -EFAULT;
			}
		}

		/*handle pointers - if offset is 0 - create unique region and store it immediately. if offset isn't 0 - find region with the same id and offset,
		 * if no such - create like for array(handle location)*/
		for (i = 0; i < p_region_element->region_element_type->pointer_type_elements_count; ++i) {
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
			int id = p_region_element->region_element_type->pointer_type_element[i]->id;

			if (json_object_get_type(p_data[id].p_json) != json_type_object) {
				msg_stderr("pointer type got invalid json type: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				free(p_data);
				return -EINVAL;
			}
			found_region_id = json_object_object_get_ex(
					p_data[id].p_json,
					"region_id",
					&p_json_region_id);
			found_offset = json_object_object_get_ex(
					p_data[id].p_json,
					"offset",
					&p_json_offset);

			if (found_region_id)children++;
			if (found_offset)children++;
			if (json_object_object_length(p_data[id].p_json) > children) {
				msg_stderr("there are not used fields: %s\n", json_object_to_json_string(p_data[id].p_json));
				/* ignore it for now
				 * free(p_data);
				 * return -EINVAL;
				 */
			}
			if (!found_region_id && found_offset) {
				msg_stderr("region_id field must present if we have offset: %s\n",
						json_object_to_json_string(p_data[id].p_json));
				free(p_data);
				return -EINVAL;
			}

			if (!found_region_id && !found_offset)	/*NULL-pointer*/
				continue;

			if (found_offset) {
				/*this is a pointer to unique region (hmm, most likely, it can by accident point to the beginning, but type will be incorrect)
				 * metac_unpack_to_json will pack it correctly. but how to check if json was build incorrectly. The problem is that we're going to
				 * create region, based on type of the pointer. What if our pointer will have incorrect type. region even won't be able to be parsed.
				 */
				continue;	/* pointer not to the unique region - will be handled later! TBD: implement it */
			}

			p_json_region = json_object_array_get_idx(p_context->p_json, json_object_get_int(p_json_region_id));

			p_array_info = metac_array_info_create_from_type(p_region_element->region_element_type->pointer_type_element[i]->type);
			if (p_array_info == NULL) {
				msg_stderr("metac_array_info_create failed - exiting\n");
				free(p_data);
				return -EFAULT;
			}
			p_array_info->subranges[0].count = json_object_array_length(p_json_region);

			if (p_region_element->region_element_type->pointer_type_element[i]->array_elements_count_funtion_ptr == NULL) {
				msg_stddbg("skipping because don't have a cb to determine elements count\n");
				metac_array_info_delete(&p_array_info);
				continue; /*we don't handle pointers if we can't get fn*/
			}

			/* now read the pointer */
			new_ptr = *((void**)(p_region_element->ptr + p_region_element->region_element_type->pointer_type_element[i]->offset));

			if (p_region_element->region_element_type->pointer_type_element[i]->array_elements_count_funtion_ptr(
					1,
					p_region_element->ptr,
					p_region_element->region_element_type->type,
					new_ptr,
					p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type?
							p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type->type:NULL,
					p_array_info,
					p_region_element->region_element_type->pointer_type_element[i]->array_elements_count_cb_context) != 0) {
				msg_stderr("Error calling array_elements_count_funtion_ptr for pointer element %d in type %s\n",
						i, p_region_element->region_element_type->type->name);
				metac_array_info_delete(&p_array_info);
				free(p_data);
				return -EFAULT;
			}

			/* calculate byte_size using length */
			elements_count = metac_array_info_get_element_count(p_array_info);
			msg_stddbg("elements_count: %d\n", (int)elements_count);
			elements_byte_size = p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type?
					metac_type_byte_size(p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type->type):0;

			if (elements_byte_size == 0 || elements_count == 0) {
				msg_stddbg("skipping because size is 0\n");
				metac_array_info_delete(&p_array_info);
				continue;
			}

			if (p_context->runtime_object->unique_region[json_object_get_int(p_json_region_id)] != NULL) {
				/*???*/
			}

			/*we have to create region and store it (need create or find to support loops) */
			_region = simply_create__region(
					p_context,
					NULL,
					elements_byte_size * elements_count,
					p_region_element->region_element_type->pointer_type_element[i]->array_elements_region_element_type,
					p_array_info,
					NULL);
			if (_region == NULL) {
				msg_stderr("Error calling find_or_create_region\n");
				metac_array_info_delete(&p_array_info);
				free(p_data);
				return -EFAULT;
			}
			_region->p_json = p_json_region;
			p_context->runtime_object->unique_region[json_object_get_int(p_json_region_id)] = _region->p_region;

			p_region_element->p_pointer[i].p_region = _region->p_region;

			/* add task to handle this region fields properly */
			if (create_and_add_runtime_task(p_breadthfirst_engine,
					p_task,
					_runtime_task_fn,
					_runtime_task_destroy_fn, _region) == NULL) {
				msg_stderr("Error calling create_and_add_runtime_task_4_region\n");
				free(p_data);
				return -EFAULT;
			}
		}
	}

	free(p_data);
	msg_stddbg("finished task\n");
	return 0;
}
/*****************************************************************************/
static struct metac_runtime_object * create_runtime_object_from_json(
		struct metac_precompiled_type * p_precompiled_type,
		json_object * p_json) {
	int i, j, k;
	metac_array_info_t * p_array_info;
	struct breadthfirst_engine* p_breadthfirst_engine;
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

	/*use breadthfirst_engine*/
	p_breadthfirst_engine = create_breadthfirst_engine();
	if (p_breadthfirst_engine == NULL){
		msg_stderr("create_breadthfirst_engine failed\n");
		free_runtime_object(&context.runtime_object);
		return NULL;
	}
	p_breadthfirst_engine->private_data = &context;

	context.p_json = p_json;
	context.runtime_object->unique_regions_count = json_object_array_length(p_json);
	assert(context.runtime_object->unique_regions_count > 0);

	context.runtime_object->unique_region = calloc(
			context.runtime_object->unique_regions_count, sizeof(*context.runtime_object->unique_region));
	if (context.runtime_object->unique_region == NULL) {
		msg_stderr("create_runtime_object failed\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	p_json_unique_region = json_object_array_get_idx(p_json, 0);
	if (json_object_get_type(p_json_unique_region) != json_type_array) {
		msg_stderr("p_json_unique_region isn't array\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	p_array_info = metac_array_info_create_from_elements_count(json_object_array_length(p_json_unique_region));
	if (p_array_info == NULL){
		msg_stderr("metac_array_info_create_from_elements_count failed\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	_region = simply_create__region(&context,
			NULL, /*right now we don't know ptr*/
			metac_type_byte_size(p_precompiled_type->region_element_type[0]->type) * metac_array_info_get_element_count(p_array_info),
			p_precompiled_type->region_element_type[0],
			p_array_info,
			NULL);
	if (_region == NULL) {
		msg_stderr("create_region failed\n");
		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}

	_region->p_json = p_json_unique_region;
	context.runtime_object->unique_region[0] = _region->p_region;

	/*add task for the first region*/
	if (create_and_add_runtime_task(p_breadthfirst_engine,
			NULL,
			_runtime_task_fn,
			_runtime_task_destroy_fn, _region) == NULL) {
		msg_stderr("add_initial_precompile_task failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);	/*region_array isn't initialized yet, runtime_object won't take care of regions*/
		}

		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
		free_runtime_object(&context.runtime_object);
		return NULL;
	}
	if (run_breadthfirst_engine(p_breadthfirst_engine, NULL) != 0) {
		msg_stderr("run_breadthfirst_engine failed\n");

		cds_list_for_each_entry(_region, &context.region_list, list) {
			delete_region(&_region->p_region);
		}

		cleanup_runtime_context(&context);
		delete_breadthfirst_engine(&p_breadthfirst_engine);
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
		delete_breadthfirst_engine(&p_breadthfirst_engine);
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

	/*TBD: to set weak pointers (we skipped them) - this will require to rework the whole file a bit: keep ptr==NULL, add jsons**to _region*/
	for (i = 0; i < context.runtime_object->regions_count; i++) {
		for (j = 0; j < context.runtime_object->region[i]->elements_count; ++j) {
			for (k = 0; k < context.runtime_object->region[i]->elements[j].region_element_type->pointer_type_elements_count; ++k) {
				if (context.runtime_object->region[i]->elements[j].p_pointer[k].p_region != NULL) {
					void ** new_ptr = ((void**)(context.runtime_object->region[i]->elements[j].ptr +
							context.runtime_object->region[i]->elements[j].region_element_type->pointer_type_element[k]->offset));
					*new_ptr = context.runtime_object->region[i]->elements[j].p_pointer[k].p_region->ptr;
				}
			}
		}
	}

	cleanup_runtime_context(&context);
	delete_breadthfirst_engine(&p_breadthfirst_engine);
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
