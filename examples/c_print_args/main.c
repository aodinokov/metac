#include <stdio.h>

#include "metac/reflect.h"



int test_function_with_args(int a, short b){
    printf("test_function_with_args: %i %hi\n", a, b);
    return a+b+6;
}

int main() {

    test_function_with_args(1, 2);

    return 0;
}