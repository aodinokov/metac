#include <stdio.h>

#include "demodb.h"

int main(){
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
