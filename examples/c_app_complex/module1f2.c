#include <stdio.h>

#include "module1.h"


static struct dup data = {
    .dup_data = 6,
};

static int _module1() {
    return printf("module1f2: %d", data.dup_data);
}

int module1f2() {
    return _module1();
}