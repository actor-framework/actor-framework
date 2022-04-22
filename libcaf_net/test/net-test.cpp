#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

#include "caf/error.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/net/host.hpp"
#include "caf/net/middleman.hpp"
#include "caf/raise_error.hpp"

int main(int argc, char** argv) {
  if (auto err = caf::net::this_host::startup())
    CAF_RAISE_ERROR(std::logic_error, "this_host::startup failed");
  using namespace caf;
  net::middleman::init_global_meta_objects();
  core::init_global_meta_objects();
  auto result = test::main(argc, argv);
  caf::net::this_host::cleanup();
  return result;
}
