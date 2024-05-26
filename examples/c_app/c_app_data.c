#include "c_app_data.h"

int data_sum(data_t * in) {
    int sum = in->numbers[0] + in->numbers[1];
    return sum;
}

int data_mul(data_t * in) {
    return in->numbers[0] * in->numbers[1];
}
