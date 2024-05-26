
#include "metac/reflect.h"
#include "cpp_app_class.h"

#include <iostream>

Vector* _v;
METAC_GSYM_LINK(_v);

int szrf = sizeof(void*&);
int szvrf = sizeof(Vector&);

int main() {
     Vector x(1,0,0), y(0,1,0), z(0,0,1);
     auto r = x+y+z;

     r.cout();

     std::cout << "sizes of reference: " << szrf << ", " << szvrf << std::endl;

     return 0;
}