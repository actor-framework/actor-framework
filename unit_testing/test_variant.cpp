#include <string>
#include <iostream>

#include "test.hpp"

#include "caf/variant.hpp"

using namespace std;
using namespace caf;

struct tostring_visitor : static_visitor<string> {
  template <class T>
  inline string operator()(const T& value) {
    return std::to_string(value);
  }
};

int main() {
  CAF_TEST(test_variant);
  tostring_visitor tv;
  // test never-empty guarantee, i.e., expect default-constucted first arg
  variant<int,float> v1;
  CAF_CHECK_EQUAL(apply_visitor(tv, v1), "0");
  variant<int,float> v2 = 42;
  CAF_CHECK_EQUAL(apply_visitor(tv, v2), "42");
  v2 = 0.2f;
  CAF_CHECK_EQUAL(apply_visitor(tv, v2), std::to_string(0.2f));
  return CAF_TEST_RESULT();
}
