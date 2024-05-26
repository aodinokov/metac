#include <stdio.h>

#include "module2.h"


struct dup data = {
    .dup_data = "some module2 data",
};

int _module1() {
    return printf("module2: %s", data.dup_data);
}

int module2() {
    return _module1();
}