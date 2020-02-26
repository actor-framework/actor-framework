#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

#include "core-test.hpp"

int main(int argc, char** argv) {
  caf::init_global_meta_objects<caf::core_test_type_ids>();
  return caf::test::main(argc, argv);
}
