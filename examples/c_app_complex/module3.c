#include <stdio.h>

#include "module3.h"


static struct dup data = {
    .dup_data = 5,
};

struct unique1_s uniq = {
    .udata = 1,
};

static int _module3() {
    int local = 1;
    do {
        int local = 2;
        printf("module3_l: %d", local);
    }while(0);
    return printf("module3: %d, %d, %d", data.dup_data, uniq.udata, local);
}

int module3() {
    return _module3();
}