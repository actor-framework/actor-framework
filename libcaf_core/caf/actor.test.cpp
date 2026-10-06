// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <functional>
#include <set>

using namespace caf;

namespace {

behavior idle() {
  return {
    [](int) {},
  };
}

struct dummy_trait {
  using signatures = type_list<result<int>(int)>;
};

using dummy_actor = typed_actor<dummy_trait>;

dummy_actor::behavior_type typed_idle() {
  return {
    [](int x) { return x; },
  };
}

} // namespace

WITH_FIXTURE(test::fixture::deterministic) {

TEST("null actor handles compare equal") {
  auto lhs = actor{};
  auto rhs = actor{nullptr};
  check(!lhs);
  check(!rhs);
  check_eq(lhs, nullptr);
  check_eq(nullptr, lhs);
  check_eq(lhs, rhs);
  check_le(lhs, rhs);
  check_ge(lhs, rhs);
  check_eq(lhs, strong_actor_ptr{});
  check_eq(strong_actor_ptr{}, lhs);
  check_eq(lhs, actor_addr{});
  check_eq(actor_addr{}, lhs);
  auto* raw = static_cast<abstract_actor*>(nullptr);
  check_eq(lhs, raw);
  check_eq(raw, lhs);
  check_eq(std::hash<actor>{}(lhs), 0u);
}

TEST("an actor handle compares equal to other views of the same actor") {
  sys.spawn([this](event_based_actor* self) {
    auto hdl = actor{self};
    auto copy = hdl;
    check_eq(hdl, copy);
    check_le(hdl, copy);
    check_ge(copy, hdl);
    check_eq(hdl, self);
    check_eq(self, hdl);
    check_le(hdl, self);
    check_ge(self, hdl);
    check_eq(hdl, self->ctrl());
    check_eq(self->ctrl(), hdl);
    check_le(hdl, self->ctrl());
    check_ge(self->ctrl(), hdl);
    check_eq(hdl, hdl.as_intrusive_ptr());
    check_eq(hdl.as_intrusive_ptr(), hdl);
    check_eq(hdl, hdl.address());
    check_eq(hdl.address(), hdl);
    check_le(hdl, hdl.address());
    check_ge(hdl.address(), hdl);
  });
}

TEST("actor handles sort by actor address") {
  // Equality uses pointer identity. Ordering uses actor_addr, so actors sort
  // by ID and node rather than by allocation address.
  auto first = sys.spawn(idle);
  auto second = sys.spawn(idle);
  check_lt(first.id(), second.id());
  check_lt(first.address(), second.address());
  check_lt(first, second);
  check_gt(second, first);
  check_ne(first, second);
  check_ne(second, first);
  check_lt(first, second.address());
  check_gt(second.address(), first);
  check_lt(first.address(), second);
  check_gt(second, first.address());
  check_lt(first, second.as_intrusive_ptr());
  check_gt(second.as_intrusive_ptr(), first);
  auto* base = actor_cast<abstract_actor*>(second);
  check_lt(first, base);
  check_gt(base, first);
  auto* derived = actor_cast<event_based_actor*>(second);
  check_lt(first, derived);
  check_gt(derived, first);
  check_lt(first, second->ctrl());
  check_gt(second->ctrl(), first);
  check_lt(actor{}, first);
  check_gt(first, actor{});
  check_lt(actor_addr{}, first);
  check_gt(first, actor_addr{});
  std::set<actor> ordered{second, first, first};
  require_eq(ordered.size(), 2u);
  check_eq(*ordered.begin(), first);
  check_eq(*ordered.rbegin(), second);
}

TEST("typed and untyped handles compare by actor address") {
  auto first = sys.spawn(typed_idle);
  auto second = sys.spawn(typed_idle);
  auto first_dyn = actor_cast<actor>(first);
  auto second_dyn = actor_cast<actor>(second);
  check_eq(first, first_dyn);
  check_eq(first_dyn, first);
  check_le(first, first_dyn);
  check_ge(first_dyn, first);
  check_eq(first, first.address());
  check_eq(first.address(), first);
  check_lt(first.id(), second.id());
  check_lt(first, second);
  check_gt(second, first);
  check_lt(first, second_dyn);
  check_gt(second_dyn, first);
  check_lt(first_dyn, second);
  check_gt(second, first_dyn);
  check_lt(first, second.address());
  check_gt(second.address(), first);
}

} // WITH_FIXTURE(test::fixture::deterministic)
