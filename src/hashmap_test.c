#include "metac/test.h"

#include "hashmap.c"
/*
going to use hashmap to find cycles.
just keeping pointers until find one existing.
also need to get the related metac_value
*/
static int _hash(void* key) {
    // key points to ptr
    return hashmapHash(&key, sizeof(void*));
    //return hashmapHash(key, sizeof(void*));
}
static bool _equals(void* keyA, void* keyB) {
    return keyA == keyB;
    //return *((int*)keyA) == *((int*)keyB);
}

static bool _callback(void* key, void* value, void* context) {
    int * p_counter = context;
    fail_unless(p_counter != NULL, "context wasn't received");
    fail_unless(key == value, "key != value");
    ++(*p_counter);
    return true;
}

METAC_START_TEST(test0_sanity) {
    struct {
        //void * ptr;
        int val;
    } data[1071];
    size_t data_len = sizeof(data)/sizeof(data[0]);
    for (int i=0; i< data_len; ++i){
        //data[i].ptr = &data[i]; 
        data[i].val = i;
    }
    Hashmap* m = hashmapCreate(110, _hash, _equals);
    fail_unless(m != NULL, "wasn't able to create hashmap");
    // write
    for (int i=0;i<data_len; ++i) {
        void * ptr = &data[i];
        //void * ptr = &data[i].ptr;
        fail_unless(hashmapContainsKey(m, ptr) == false, "map reports that the key exists %d", i);
        fail_unless(hashmapPut(m, ptr, ptr, NULL) == 0, "couldn't add to map %d", i);
    }

    // read
    for (int i = 0; i < data_len; ++i) {
        void * ptr = &data[i];
        //void * ptr = &data[i].ptr;
        fail_unless(hashmapContainsKey(m, ptr) == true, "map reports that the key doesn't exist");
        void *val = hashmapGet(m,ptr);
        fail_unless(ptr == val, "map returned something incorrect %p, expectd %p", val, ptr);
    }
    // len
    size_t len = hashmapSize(m);
    fail_unless(len == data_len, "got len %lu, expected %lu", len, data_len);

    int counter = 0;
    hashmapForEach(m, _callback, &counter);
    fail_unless(counter == data_len, "got counter %i, expected %lu", counter, data_len);

    // remove
    for (int i = 0; i < data_len; ++i) {
        void * ptr = &data[i];
        //void * ptr = &data[i].ptr;
        hashmapRemove(m, ptr);
    }

    len = hashmapSize(m);
    fail_unless(len == 0, "got len %lu, expected %u", len, 0);

    hashmapFree(m);
}END_TEST