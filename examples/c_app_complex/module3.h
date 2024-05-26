#ifndef MODULE3_H
#define MODULE3_H

typedef struct unique1_s  unique1_t;

struct unique1_s {
    int udata;
    unique1_t *p;
};

struct dup {
    int dup_data;
};

int module3();

#endif