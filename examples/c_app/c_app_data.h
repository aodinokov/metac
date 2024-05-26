#ifndef C_APP_H_
#define C_APP_H_

typedef struct {
    int numbers[2];
}data_t;

int data_sum(data_t * in);
int data_mul(data_t * in);

#endif /* C_APP_H_ */
