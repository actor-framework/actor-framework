#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

#include "caf/init_global_meta_objects.hpp"
#include "caf/net/middleman.hpp"

int main(int argc, char** argv) {
  using namespace caf;
  net::middleman::init_global_meta_objects();
  core::init_global_meta_objects();
  return test::main(argc, argv);
}
