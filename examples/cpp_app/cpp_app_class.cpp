#include "cpp_app_class.h"

#include <iostream>

using namespace std;

Vector operator+(const Vector &a, const Vector &b) {
  Vector r;
  r.x = a.x + b.x;
  r.y = a.y + b.y;
  r.z = a.z + b.z;
  return r;
}

void Vector::cout() {
  std::cout << "Vector: " << x << ", " << y << ", " << z << endl;
}
