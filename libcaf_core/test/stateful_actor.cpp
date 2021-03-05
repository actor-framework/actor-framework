// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE stateful_actor

#include "caf/stateful_actor.hpp"

#include "core-test.hpp"

#include "caf/event_based_actor.hpp"

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

struct fixture : test_coordinator_fixture<> {
  fixture() {
    // nop
  }

  template <class ActorUnderTest>
  void test_adder(ActorUnderTest aut) {
    inject((add_atom, int), from(self).to(aut).with(add_atom_v, 7));
    inject((add_atom, int), from(self).to(aut).with(add_atom_v, 4));
    inject((add_atom, int), from(self).to(aut).with(add_atom_v, 9));
    inject((get_atom), from(self).to(aut).with(get_atom_v));
    expect((int), from(aut).to(self).with(20));
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
    inject((get_atom), from(self).to(aut).with(get_atom_v));
    expect((std::string), from(aut).to(self).with(expected));
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(dynamic_stateful_actor_tests, fixture)

CAF_TEST(stateful actors can be dynamically typed) {
  test_adder(sys.spawn(adder));
  test_adder(sys.spawn<typed_adder_class>());
}

CAF_TEST(stateful actors can be statically typed) {
  test_adder(sys.spawn(typed_adder));
  test_adder(sys.spawn<adder_class>());
}

CAF_TEST(stateful actors without explicit name use the name of the parent) {
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

CAF_TEST(states with static C string names override the default name) {
  test_name<named_state>("testee");
}

namespace {

int32_t add_operation(int32_t x, int32_t y) {
  return x + y;
}

} // namespace

CAF_TEST(states can accept constructor arguments and provide a behavior) {
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
        [=](int32_t x, int32_t y) {
          this->x = x;
          this->y = y;
        },
        [=](get_atom) { return f(x, y); },
      };
    }
  };
  using actor_type = stateful_actor<state_type>;
  auto testee = sys.spawn<actor_type>(10, 20, add_operation);
  auto& state = deref<actor_type>(testee).state;
  CAF_CHECK_EQUAL(state.x, 10);
  CAF_CHECK_EQUAL(state.y, 20);
  inject((get_atom), from(self).to(testee).with(get_atom_v));
  expect((int32_t), from(testee).to(self).with(30));
  inject((int32_t, int32_t), to(testee).with(1, 2));
  CAF_CHECK_EQUAL(state.x, 1);
  CAF_CHECK_EQUAL(state.y, 2);
  inject((get_atom), from(self).to(testee).with(get_atom_v));
  expect((int32_t), from(testee).to(self).with(3));
}

CAF_TEST(states optionally take the self pointer as first argument) {
  struct state_type : named_state {
    event_based_actor* self;
    int x;
    state_type(event_based_actor* self, int x) : self(self), x(x) {
      // nop
    }
    behavior make_behavior() {
      return {
        [=](get_atom) { return self->name(); },
      };
    }
  };
  using actor_type = stateful_actor<state_type>;
  auto testee = sys.spawn<actor_type>(10);
  auto& state = deref<actor_type>(testee).state;
  CAF_CHECK(state.self == &deref<actor_type>(testee));
  CAF_CHECK_EQUAL(state.x, 10);
  inject((get_atom), from(self).to(testee).with(get_atom_v));
  expect((std::string), from(testee).to(self).with("testee"s));
}

CAF_TEST(typed actors can use typed_actor_pointer as self pointer) {
  struct state_type : named_state {
    using self_pointer = typed_adder_actor::pointer_view;
    self_pointer self;
    int value;
    state_type(self_pointer self, int x) : self(self), value(x) {
      // nop
    }
    auto make_behavior() {
      return make_typed_behavior([=](add_atom, int x) { value += x; },
                                 [=](get_atom) { return value; });
    }
  };
  using actor_type = typed_adder_actor::stateful_impl<state_type>;
  auto testee = sys.spawn<actor_type>(10);
  auto& state = deref<actor_type>(testee).state;
  CAF_CHECK(state.self == &deref<actor_type>(testee));
  CAF_CHECK_EQUAL(state.value, 10);
  inject((add_atom, int), from(self).to(testee).with(add_atom_v, 1));
  inject((get_atom), from(self).to(testee).with(get_atom_v));
  expect((int), from(testee).to(self).with(11));
}

CAF_TEST(returned behaviors take precedence over make_behavior in the state) {
  struct state_type : named_state {
    behavior make_behavior() {
      CAF_LOG_TRACE("");
      return {
        [](int32_t x, int32_t y) { return x - y; },
      };
    }
  };
  auto fun = [](stateful_actor<state_type>*, int32_t num) -> behavior {
    CAF_LOG_TRACE(CAF_ARG(num));
    return {
      [num](int32_t x, int32_t y) { return x + y + num; },
    };
  };
  auto testee = sys.spawn<lazy_init>(fun, 10);
  inject((int32_t, int32_t), from(self).to(testee).with(1, 2));
  expect((int32_t), from(testee).to(self).with(13));
}

CAF_TEST_FIXTURE_SCOPE_END()
