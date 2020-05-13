// This example is a very basic, non-interactive math service implemented for
// both the blocking and the event-based API.

#include <iostream>

#include "caf/all.hpp"

using std::endl;
using namespace caf;

namespace {

// --(rst-actor-begin)--
using calculator_actor
  = typed_actor<replies_to<add_atom, int32_t, int32_t>::with<int32_t>,
                replies_to<sub_atom, int32_t, int32_t>::with<int32_t>>;
// --(rst-actor-end)--

// --(rst-fwd-begin)--
// prototypes and forward declarations
behavior calculator_fun(event_based_actor* self);
void blocking_calculator_fun(blocking_actor* self);
calculator_actor::behavior_type typed_calculator_fun();
class calculator;
class blocking_calculator;
class typed_calculator;
// --(rst-fwd-end)--

// --(rst-funs-begin)--
// function-based, dynamically typed, event-based API
behavior calculator_fun(event_based_actor*) {
  return {
    [](add_atom, int32_t a, int32_t b) { return a + b; },
    [](sub_atom, int32_t a, int32_t b) { return a - b; },
  };
}

// function-based, dynamically typed, blocking API
void blocking_calculator_fun(blocking_actor* self) {
  bool running = true;
  self->receive_while(running)( //
    [](add_atom, int32_t a, int32_t b) { return a + b; },
    [](sub_atom, int32_t a, int32_t b) { return a - b; },
    [&](exit_msg& em) {
      if (em.reason) {
        self->fail_state(std::move(em.reason));
        running = false;
      }
    });
}

// function-based, statically typed, event-based API
calculator_actor::behavior_type typed_calculator_fun() {
  return {
    [](add_atom, int32_t a, int32_t b) { return a + b; },
    [](sub_atom, int32_t a, int32_t b) { return a - b; },
  };
}
// --(rst-funs-end)--

// --(rst-classes-begin)--
// class-based, dynamically typed, event-based API
class calculator : public event_based_actor {
public:
  calculator(actor_config& cfg) : event_based_actor(cfg) {
    // nop
  }

  behavior make_behavior() override {
    return calculator_fun(this);
  }
};

// class-based, dynamically typed, blocking API
class blocking_calculator : public blocking_actor {
public:
  blocking_calculator(actor_config& cfg) : blocking_actor(cfg) {
    // nop
  }

  void act() override {
    blocking_calculator_fun(this);
  }
};

// class-based, statically typed, event-based API
class typed_calculator : public calculator_actor::base {
public:
  typed_calculator(actor_config& cfg) : calculator_actor::base(cfg) {
    // nop
  }

  behavior_type make_behavior() override {
    return typed_calculator_fun();
  }
};
// --(rst-classes-end)--

void tester(scoped_actor&) {
  // end of recursion
}

// tests a calculator instance
template <class Handle, class... Ts>
void tester(scoped_actor& self, const Handle& hdl, int32_t x, int32_t y,
            Ts&&... xs) {
  auto handle_err = [&](const error& err) {
    aout(self) << "AUT (actor under test) failed: " << to_string(err) << endl;
  };
  // first test: x + y = z
  self->request(hdl, infinite, add_atom_v, x, y)
    .receive(
      [&](int32_t res1) {
        aout(self) << x << " + " << y << " = " << res1 << endl;
        // second test: x - y = z
        self->request(hdl, infinite, sub_atom_v, x, y)
          .receive(
            [&](int32_t res2) {
              aout(self) << x << " - " << y << " = " << res2 << endl;
            },
            handle_err);
      },
      handle_err);
  tester(self, std::forward<Ts>(xs)...);
}

void caf_main(actor_system& system) {
  // --(rst-spawn-begin)--
  auto a1 = system.spawn(blocking_calculator_fun);
  auto a2 = system.spawn(calculator_fun);
  auto a3 = system.spawn(typed_calculator_fun);
  auto a4 = system.spawn<blocking_calculator>();
  auto a5 = system.spawn<calculator>();
  auto a6 = system.spawn<typed_calculator>();
  // --(rst-spawn-end)--
  scoped_actor self{system};
  tester(self, a1, 1, 2, a2, 3, 4, a3, 5, 6, a4, 7, 8, a5, 9, 10, a6, 11, 12);
  self->send_exit(a1, exit_reason::user_shutdown);
  self->send_exit(a4, exit_reason::user_shutdown);
}

} // namespace

CAF_MAIN()
