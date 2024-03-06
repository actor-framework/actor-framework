#include "net-test.hpp"

#include "caf/net/middleman.hpp"
#include "caf/net/ssl/startup.hpp"
#include "caf/net/this_host.hpp"

#include "caf/error.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/raise_error.hpp"

#define CAF_TEST_NO_MAIN

#include "caf/test/unit_test_impl.hpp"

using namespace caf;

// -- barrier ------------------------------------------------------------------

void barrier::arrive_and_wait() {
  std::unique_lock<std::mutex> guard{mx_};
  auto new_count = ++count_;
  if (new_count == num_threads_) {
    cv_.notify_all();
  } else if (new_count > num_threads_) {
    count_ = 1;
    cv_.wait(guard, [this] { return count_.load() == num_threads_; });
  } else {
    cv_.wait(guard, [this] { return count_.load() == num_threads_; });
  }
}

// -- main --------------------------------------------------------------------

int main(int argc, char** argv) {
  net::this_host::startup();
  net::ssl::startup();
  using namespace caf;
  net::middleman::init_global_meta_objects();
  core::init_global_meta_objects();
  auto result = test::main(argc, argv);
  net::ssl::cleanup();
  net::this_host::cleanup();
  return result;
}
