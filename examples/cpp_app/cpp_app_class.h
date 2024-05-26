
class Vector {
private:
  double x,y,z;
public:
  Vector() : x(0), y(0), z(0) { }
  Vector(double x, double y, double z) : x(x), y(y), z(z) { }
  friend Vector operator+(const Vector &a, const Vector &b);
  void cout();
};
