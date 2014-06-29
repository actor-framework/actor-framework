#include <string>
#include <iostream>

#include "test.hpp"

#include "cppa/variant.hpp"

using namespace std;
using namespace cppa;

struct tostring_visitor : static_visitor<string> {
    template<typename T>
    inline string operator()(const T& value) {
        return std::to_string(value);
    }
};

int main() {
    CPPA_TEST(test_variant);
    tostring_visitor tv;
    // test never-empty guarantee, i.e., expect default-constucted first arg
    variant<int,float> v1;
    CPPA_CHECK_EQUAL(apply_visitor(tv, v1), "0");
    variant<int,float> v2 = 42;
    CPPA_CHECK_EQUAL(apply_visitor(tv, v2), "42");
    v2 = 0.2f;
    CPPA_CHECK_EQUAL(apply_visitor(tv, v2), std::to_string(0.2f));
    return CPPA_TEST_RESULT();
}
