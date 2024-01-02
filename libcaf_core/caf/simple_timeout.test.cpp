// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/after.hpp"
#include "caf/all.hpp"
#include "caf/log/test.hpp"

#include <chrono>
#include <memory>

using namespace caf;
using namespace std::literals;

namespace {

using ms = std::chrono::milliseconds;

using timer = typed_actor<result<void>(reset_atom)>;

struct timer_state {
  bool had_reset = false;
};

timer::behavior_type timer_impl(timer::stateful_pointer<timer_state> self) {
  self->delayed_send(self, 100ms, reset_atom_v);
  return {
    [=](reset_atom) {
      log::test::debug("timer reset");
      self->state.had_reset = true;
    },
    after(600ms) >>
      [=] {
        log::test::debug("timer expired");
        test::runnable::current().require(self->state.had_reset);
        self->quit();
      },
  };
}

timer::behavior_type timer_impl2(timer::pointer self) {
  auto had_reset = std::make_shared<bool>(false);
  delayed_anon_send(self, 100ms, reset_atom_v);
  return {
    [=](reset_atom) {
      log::test::debug("timer reset");
      *had_reset = true;
    },
    after(600ms) >>
      [=] {
        log::test::debug("timer expired");
        test::runnable::current().require(*had_reset);
        self->quit();
      },
  };
}

WITH_FIXTURE(test::fixture::deterministic) {

TEST("single_timeout") {
  sys.spawn(timer_impl);
  advance_time(100ms);
  dispatch_messages();
  advance_time(600ms);
  dispatch_messages();
}

TEST("single_anon_timeout") {
  sys.spawn(timer_impl2);
  advance_time(100ms);
  dispatch_messages();
  advance_time(600ms);
  dispatch_messages();
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
