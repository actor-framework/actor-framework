#include "core-test.hpp"

#include "caf/test/unit_test_impl.hpp"

fail_on_copy::fail_on_copy(const fail_on_copy&) {
  CAF_FAIL("fail_on_copy: copy constructor called");
}

fail_on_copy& fail_on_copy::operator=(const fail_on_copy&) {
  CAF_FAIL("fail_on_copy: copy assign operator called");
}

size_t i32_wrapper::instances = 0;

size_t i64_wrapper::instances = 0;

void test_empty_non_pod::foo() {
  // nop
}

test_empty_non_pod::~test_empty_non_pod() {
  // nop
}

namespace {

const char* test_enum_strings[] = {
  "a",
  "b",
  "c",
};

} // namespace

std::string to_string(test_enum x) {
  return test_enum_strings[static_cast<uint32_t>(x)];
}
