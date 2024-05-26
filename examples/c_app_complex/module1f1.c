#include <stdio.h>

#include "module1.h"


static struct dup data = {
    .dup_data = 5,
};

static int _module1() {
    return printf("module1f1: %d", data.dup_data);
}

int module1f1() {
    return _module1();
}