#include <stdio.h>
#include <stdlib.h>

#include "demodb.h"

#include "metac/reflect.h"

person_t * p_person = NULL;
METAC_GSYM_LINK(p_person);
METAC_GSYM_LINK(db_append);

int main(){
	metac_entry_t *p_person_entry = METAC_GSYM_LINK_ENTRY(p_person);
	if (p_person_entry == NULL) {
		printf("wasn't able to get p_person_entry type information\n");
		return 1;
	}
	metac_entry_t *p_db_append_entry = METAC_GSYM_LINK_ENTRY(db_append);
	if (p_db_append_entry == NULL) {
		printf("wasn't able to get db_append type information\n");
		return 1;
	}
	char * p_cdecl = NULL;
	p_cdecl = metac_entry_cdecl(p_person_entry);
	if (p_cdecl == NULL) {
		return 1;
	}
	printf("p_person_entry: %s\n", p_cdecl);
	free(p_cdecl);
	p_cdecl = metac_entry_cdecl(p_db_append_entry);
	if (p_cdecl == NULL) {
		return 1;
	}
	printf("p_db_append_entry: %s\n", p_cdecl);
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
