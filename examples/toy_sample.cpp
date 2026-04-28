#include <iostream>

namespace toy {

int add(int lhs, int rhs) {
  return lhs + rhs;
}

static int multiply(int lhs, int rhs) {
  return lhs * rhs;
}

class Calculator {
public:
  int subtract(int lhs, int rhs) const {
    return lhs - rhs;
  }
};

} // namespace toy

int main() {
  std::cout << toy::add(2, 3) << '\n';
  std::cout << toy::multiply(2, 3) << '\n';
  toy::Calculator calculator;
  std::cout << calculator.subtract(8, 5) << '\n';
  return 0;
}
