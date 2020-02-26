#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

#include "io-test.hpp"

int main(int argc, char** argv) {
  caf::init_global_meta_objects<caf::io_test_type_ids>();
  caf::io::middleman::init_global_meta_objects();
  return caf::test::main(argc, argv);
}
