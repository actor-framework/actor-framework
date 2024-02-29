// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/stateful_actor.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_actor_pointer.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;

using namespace std::string_literals;

namespace {

using typed_adder_actor
  = typed_actor<result<void>(add_atom, int), result<int>(get_atom)>;

struct counter {
  int value = 0;
};

behavior adder(stateful_actor<counter>* self) {
  return {
    [=](add_atom, int x) { self->state.value += x; },
    [=](get_atom) { return self->state.value; },
  };
}

class adder_class : public stateful_actor<counter> {
public:
  adder_class(actor_config& cfg) : stateful_actor<counter>(cfg) {
    // nop
  }

  behavior make_behavior() override {
    return adder(this);
  }
};

typed_adder_actor::behavior_type
typed_adder(typed_adder_actor::stateful_pointer<counter> self) {
  return {
    [=](add_atom, int x) { self->state.value += x; },
    [=](get_atom) { return self->state.value; },
  };
}

class typed_adder_class : public typed_adder_actor::stateful_impl<counter> {
public:
  using super = typed_adder_actor::stateful_impl<counter>;

  typed_adder_class(actor_config& cfg) : super(cfg) {
    // nop
  }

  behavior_type make_behavior() override {
    return typed_adder(this);
  }
};

struct fixture : test::fixture::deterministic {
  scoped_actor self{sys};
  fixture() {
    // nop
  }

  template <class ActorUnderTest>
  void test_adder(ActorUnderTest aut) {
    inject().with(add_atom_v, 7).from(self).to(aut);
    inject().with(add_atom_v, 4).from(self).to(aut);
    inject().with(add_atom_v, 9).from(self).to(aut);
    inject().with(get_atom_v).from(self).to(aut);
    auto received = std::make_shared<bool>(false);
    self->receive([received](int received_int) {
      *received = true;
      test::runnable::current().check_eq(received_int, 20);
    });
    test::runnable::current().check(*received);
  }

  template <class State>
  void test_name(const char* expected) {
    auto aut = sys.spawn([](stateful_actor<State>* self) -> behavior {
      return {
        [=](get_atom) {
          self->quit();
          return self->name();
        },
      };
    });
    inject().with(get_atom_v).from(self).to(aut);
    auto received = std::make_shared<bool>(false);
    self->receive([received, expected](std::string received_str) {
      *received = true;
      test::runnable::current().check_eq(expected, received_str);
    });
    test::runnable::current().check(*received);
  }

  template <class T = caf::scheduled_actor, class Handle = caf::actor>
  T& deref(const Handle& hdl) {
    auto ptr = caf::actor_cast<caf::abstract_actor*>(hdl);
    test::runnable::current().require(ptr != nullptr);
    return dynamic_cast<T&>(*ptr);
  }
};

WITH_FIXTURE(fixture) {

TEST("stateful actors can be dynamically typed") {
  test_adder(sys.spawn(adder));
  test_adder(sys.spawn<typed_adder_class>());
}

TEST("stateful actors can be statically typed") {
  test_adder(sys.spawn(typed_adder));
  test_adder(sys.spawn<adder_class>());
}

TEST("stateful actors without explicit name use the name of the parent") {
  struct state {
    // empty
  };
  test_name<state>("user.scheduled-actor");
}

namespace {

struct named_state {
  static inline const char* name = "testee";
};

} // namespace

TEST("states with static C string names override the default name") {
  test_name<named_state>("testee");
}

namespace {

int32_t add_operation(int32_t x, int32_t y) {
  return x + y;
}

} // namespace

TEST("states can accept constructor arguments and provide a behavior") {
  struct state_type {
    using operation_type = int32_t (*)(int32_t, int32_t);
    int32_t x;
    int32_t y;
    operation_type f;
    state_type(int32_t x, int32_t y, operation_type f) : x(x), y(y), f(f) {
      // nop
    }
    behavior make_behavior() {
      return {
        [this](int32_t x, int32_t y) {
          this->x = x;
          this->y = y;
        },
        [this](get_atom) { return f(x, y); },
      };
    }
  };
  using actor_type = stateful_actor<state_type>;
  auto testee = sys.spawn<actor_type>(10, 20, add_operation);
  auto& state = deref<actor_type>(testee).state;
  check_eq(state.x, 10);
  check_eq(state.y, 20);
  inject().with(get_atom_v).from(self).to(testee);
  auto received = std::make_shared<bool>(false);
  self->receive([this, received](int received_int) {
    *received = true;
    check_eq(received_int, 30);
  });
  check(*received);
  inject().with(1, 2).to(testee);
  check_eq(state.x, 1);
  check_eq(state.y, 2);
  inject().with(get_atom_v).from(self).to(testee);
  *received = false;
  self->receive([this, received](int received_int) {
    *received = true;
    check_eq(received_int, 3);
  });
  check(*received);
}

TEST("states optionally take the self pointer as first argument") {
  struct state_type : named_state {
    event_based_actor* self;
    int x;
    state_type(event_based_actor* self, int x) : self(self), x(x) {
      // nop
    }
    behavior make_behavior() {
      return {
        [this](get_atom) { return self->name(); },
      };
    }
  };
  using actor_type = stateful_actor<state_type>;
  auto testee = sys.spawn<actor_type>(10);
  auto& state = deref<actor_type>(testee).state;
  check(state.self == &deref<actor_type>(testee));
  check_eq(state.x, 10);
  inject().with(get_atom_v).from(self).to(testee);
  auto received = std::make_shared<bool>(false);
  self->receive([this, received](std::string received_str) {
    *received = true;
    check_eq("testee", received_str);
  });
  check(*received);
}

TEST("typed actors can use typed_actor_pointer as self pointer") {
  struct state_type : named_state {
    using self_pointer = typed_adder_actor::pointer_view;
    self_pointer self;
    int value;
    state_type(self_pointer self, int x) : self(self), value(x) {
      // nop
    }
    auto make_behavior() {
      return make_typed_behavior([this](add_atom, int x) { value += x; },
                                 [this](get_atom) { return value; });
    }
  };
  using actor_type = typed_adder_actor::stateful_impl<state_type>;
  auto testee = sys.spawn<actor_type>(10);
  auto& state = deref<actor_type>(testee).state;
  check(state.self == &deref<actor_type>(testee));
  check_eq(state.value, 10);
  inject().with(add_atom_v, 1).from(self).to(testee);
  inject().with(get_atom_v).from(self).to(testee);
  auto received = std::make_shared<bool>(false);
  self->receive([this, received](int received_int) {
    *received = true;
    check_eq(received_int, 11);
  });
}

TEST("returned behaviors take precedence over make_behavior in the state") {
  struct state_type : named_state {
    behavior make_behavior() {
      auto lg = log::core::trace("");
      return {
        [](int32_t x, int32_t y) { return x - y; },
      };
    }
  };
  auto fun = [](stateful_actor<state_type>*, int32_t num) -> behavior {
    auto lg = log::core::trace("num = {}", num);
    return {
      [num](int32_t x, int32_t y) { return x + y + num; },
    };
  };
  auto testee = sys.spawn<lazy_init>(fun, 10);
  inject().with(1, 2).from(self).to(testee);
  auto received = std::make_shared<bool>(false);
  self->receive([this, received](int received_int) {
    *received = true;
    check_eq(received_int, 13);
  });
}

} // WITH_FIXTURE(fixture)

} // namespace
