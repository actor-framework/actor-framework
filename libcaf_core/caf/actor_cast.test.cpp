// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_cast.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;

namespace {

struct dummy_trait {
  using signatures = type_list<result<int>(int)>;
};

using dummy_actor = typed_actor<dummy_trait>;

behavior dummy_impl() {
  return {
    [](int x) { return x; },
  };
}

dummy_actor::behavior_type typed_dummy_impl() {
  return {
    [](int x) { return x; },
  };
}

} // namespace

WITH_FIXTURE(test::fixture::deterministic) {

TEST("actor_cast converts between strong and weak pointers") {
  SECTION("valid handle") {
    SECTION("actor") {
      auto hdl = sys.spawn(dummy_impl);
      auto ptr = actor_cast<weak_actor_ptr>(hdl);
      check_eq(hdl.compare(ptr.ctrl()), 0);
    }
    SECTION("typed_actor") {
      auto hdl = sys.spawn(typed_dummy_impl);
      auto ptr = actor_cast<weak_actor_ptr>(hdl);
      check_eq(hdl.compare(ptr.ctrl()), 0);
    }
  }
  SECTION("invalid handle") {
    SECTION("actor") {
      auto hdl = actor{};
      auto ptr = actor_cast<weak_actor_ptr>(hdl);
      check_eq(ptr.compare(nullptr), 0);
    }
    SECTION("typed_actor") {
      auto hdl = dummy_actor{};
      auto ptr = actor_cast<weak_actor_ptr>(hdl);
      check_eq(ptr.compare(nullptr), 0);
    }
  }
}

TEST("actor_cast converts a weak pointer back to a handle") {
  SECTION("valid handle") {
    SECTION("actor") {
      auto hdl = sys.spawn(dummy_impl);
      auto wptr = actor_cast<weak_actor_ptr>(hdl);
      auto hdl2 = actor_cast<actor>(wptr);
      check_eq(hdl.compare(wptr.ctrl()), 0);
      check_eq(hdl2.compare(hdl), 0);
    }
    SECTION("typed_actor") {
      auto hdl = sys.spawn(typed_dummy_impl);
      auto wptr = actor_cast<weak_actor_ptr>(hdl);
      auto hdl2 = actor_cast<dummy_actor>(wptr);
      check_eq(hdl.compare(wptr.ctrl()), 0);
      check_eq(hdl2.compare(hdl), 0);
    }
  }
  SECTION("expired handle") {
    SECTION("actor") {
      auto wptr = actor_cast<weak_actor_ptr>(sys.spawn(dummy_impl));
      auto hdl2 = actor_cast<actor>(wptr);
      check_eq(hdl2.compare(nullptr), 0);
    }
    SECTION("typed_actor") {
      auto wptr = actor_cast<weak_actor_ptr>(sys.spawn(typed_dummy_impl));
      auto hdl2 = actor_cast<dummy_actor>(wptr);
      check_eq(hdl2.compare(nullptr), 0);
    }
  }
}

TEST("actor_cast converts a self pointer to a handle type") {
  SECTION("actor") {
    sys.spawn([this](event_based_actor* self) {
      auto hdl = actor_cast<actor>(self);
      check_eq(hdl.compare(self->ctrl()), 0);
    });
    auto* ptr = static_cast<event_based_actor*>(nullptr);
    auto hdl = actor_cast<actor>(ptr);
    check_eq(hdl.compare(nullptr), 0);
  }
  SECTION("typed_actor") {
    sys.spawn([this](dummy_actor::pointer self) {
      auto hdl = actor_cast<dummy_actor>(self);
      check_eq(hdl.compare(self->ctrl()), 0);
      return dummy_actor::behavior_type{
        [](int x) { return x; },
      };
    });
    auto* ptr = static_cast<dummy_actor::pointer>(nullptr);
    auto hdl = actor_cast<actor>(ptr);
    check_eq(hdl.compare(nullptr), 0);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
