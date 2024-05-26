#include <stdio.h>
#include <stdlib.h>

#include "demodb.h"

#include "metac/reflect.h"

WITH_METAC_DECLLOC(gl_dl1,
int some_function(int x) {
	return -1;
})

WITH_METAC_DECLLOC(gl_dl2, person_t * p_gl_person = NULL);

int main(){
	WITH_METAC_DECLLOC(dl, person_t * p_person = NULL;
	// We even can use METAC_ENTRY_FROM_DECLLOC inside WITH_METAC_DECLLOC
	metac_entry_t *p_person_entry = METAC_ENTRY_FROM_DECLLOC(dl, p_person));

	metac_entry_t *p_gl_some_function = METAC_ENTRY_FROM_DECLLOC(gl_dl1, some_function);
	metac_entry_t *p_gl_person_entry = METAC_ENTRY_FROM_DECLLOC(gl_dl2, p_gl_person);

	// we're getting the same type information from p_person_entry and p_gl_some_function
	if (metac_entry_final_entry(p_person_entry, NULL) == NULL ||
		metac_entry_final_entry(p_person_entry, NULL) != metac_entry_final_entry(p_gl_person_entry, NULL)) {
			return 1;
	}

	char * p_cdecl = NULL;
	p_cdecl = metac_entry_cdecl(p_person_entry);
	if (p_cdecl == NULL) {
		return 1;
	}
	printf("p_person_entry: %s\n", p_cdecl);
	free(p_cdecl);
	p_cdecl = metac_entry_cdecl(p_gl_some_function);
	if (p_cdecl == NULL) {
		return 1;
	}
	printf("p_gl_some_function: %s\n", p_cdecl);
	free(p_cdecl);

	if (metac_entry_is_pointer(p_person_entry)==0) {
		return 1;
	}
	metac_entry_t *p_person_typedef = metac_entry_pointer_entry(p_person_entry);
	p_cdecl = metac_entry_cdecl(p_person_typedef);
	if (p_cdecl == NULL) {
		return 1;
	}
	printf("p_person_typedef: %s\n", p_cdecl);
	free(p_cdecl);

	metac_entry_t *p_person_struct = metac_entry_final_entry(p_person_typedef, NULL);
	p_cdecl = metac_entry_cdecl(p_person_struct);
	if (p_cdecl == NULL) {
		return 1;
	}
	printf("p_person_struct: ");
	printf(p_cdecl, "");
	printf("\n");
	free(p_cdecl);

	db_t * p_db =  new_db();

	if (p_db == NULL) {
		return 1;
	}

	db_append(&p_db, (person_t[]){{
		.firstname="Joe",
		.lastname="Doe",
		.age = 43,
		.marital_status = msMarried,
	}});
	db_delete_record(p_db, 0);

	db_delete(p_db);
	return 0;
}
