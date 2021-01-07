// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE simple_timeout

#include "caf/after.hpp"

#include "core-test.hpp"

#include <chrono>
#include <memory>

#include "caf/all.hpp"

using namespace caf;

namespace {

using ms = std::chrono::milliseconds;

using timer = typed_actor<reacts_to<reset_atom>>;

struct timer_state {
  bool had_reset = false;
};

timer::behavior_type timer_impl(timer::stateful_pointer<timer_state> self) {
  self->delayed_send(self, ms(100), reset_atom_v);
  return {
    [=](reset_atom) {
      CAF_MESSAGE("timer reset");
      self->state.had_reset = true;
    },
    after(ms(600)) >>
      [=] {
        CAF_MESSAGE("timer expired");
        CAF_REQUIRE(self->state.had_reset);
        self->quit();
      },
  };
}

timer::behavior_type timer_impl2(timer::pointer self) {
  auto had_reset = std::make_shared<bool>(false);
  delayed_anon_send(self, ms(100), reset_atom_v);
  return {
    [=](reset_atom) {
      CAF_MESSAGE("timer reset");
      *had_reset = true;
    },
    after(ms(600)) >>
      [=] {
        CAF_MESSAGE("timer expired");
        CAF_REQUIRE(*had_reset);
        self->quit();
      },
  };
}

} // namespace

CAF_TEST_FIXTURE_SCOPE(simple_timeout_tests, test_coordinator_fixture<>)

CAF_TEST(single_timeout) {
  sys.spawn(timer_impl);
}

CAF_TEST(single_anon_timeout) {
  sys.spawn(timer_impl2);
}

CAF_TEST_FIXTURE_SCOPE_END()
