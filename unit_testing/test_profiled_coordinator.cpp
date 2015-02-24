#include "test.hpp"

#include "caf/scheduler/profiled_coordinator.hpp"

using namespace caf;

int main() {
  CAF_TEST(test_profiled_coordinator);
  set_scheduler(new scheduler::profiled_coordinator<>{"/dev/null"});
  return 0;
}
