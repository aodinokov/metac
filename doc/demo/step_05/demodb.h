
typedef struct {
	char * firstname;
	char * lastname;
	int age;
	enum {
		msSingle = 0,
		msMarried,
		msDivorsed,
	} marital_status;
}person_t;

typedef struct db db_t;

db_t * new_db();
void db_delete(db_t *p_db);
int db_append(db_t ** pp_db, person_t * p_person);
int db_delete_record(db_t * p_db, int inx);
