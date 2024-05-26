#include <errno.h> /* ENOMEM */
#include <stdlib.h> /* calloc */
#include <string.h> /* memcpy */

#include "demodb.h"

typedef struct db {
	int count;
	person_t data[];
}db_t;

db_t * new_db() {
	return calloc(1, sizeof(db_t));	
}

void db_delete(db_t * p_db) {
	if (p_db == NULL) {
		return;
	}

	for (int i = 0 ; i < p_db->count; ++i) {
		if (p_db->data[i].firstname != NULL) {
			free(p_db->data[i].firstname);
		}
		if (p_db->data[i].lastname != NULL) {
			free(p_db->data[i].lastname);
		}
	}

	free(p_db);
}

int db_append(db_t ** pp_db, person_t * p_person) {
	if (pp_db == NULL || (*pp_db) == NULL || p_person == NULL) {
		return -(EINVAL);
	}
	db_t * p_db = *pp_db;

	int inx = p_db->count;
	size_t new_sz = ((char*)&p_db->data[inx+2]) - ((char*)p_db); // similar to __offsetof(db_t, data[inx+2])
	p_db = realloc(p_db, new_sz);	// +2 because we need beginning of the record next to what we allocating
	if (p_db == NULL) {
		return -(ENOMEM);
	}
	*pp_db = p_db;

	memcpy(&p_db->data[inx], p_person, sizeof(person_t));
	if (p_person->firstname != NULL) {
		p_db->data[inx].firstname = strdup(p_person->firstname);
		if (p_db->data[inx].firstname == NULL) {
			return -(ENOMEM);
		}
	}
	if (p_person->lastname != NULL) {
		p_db->data[inx].lastname = strdup(p_person->lastname);
		if (p_db->data[inx].lastname == NULL) {
			if (p_db->data[inx].firstname != NULL) {
				free(p_db->data[inx].firstname);
			}
			return -(ENOMEM);
		}
	}
	++p_db->count;
	return 0;
}

int db_delete_record(db_t * p_db, int inx) {
	if (p_db == NULL || inx < 0 || inx >= p_db->count) {
		return -(EINVAL);
	}

	if (p_db->data[inx].firstname != NULL) {
		free(p_db->data[inx].firstname);
	}
	if (p_db->data[inx].lastname != NULL) {
		free(p_db->data[inx].lastname);
	}

	if (inx != p_db->count - 1) {
		memcpy(&p_db->data[inx], &p_db->data[inx+1], sizeof(person_t) * (p_db->count - 1 - inx));
	}

	--p_db->count;
	return 0;
}
