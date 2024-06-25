// This example is a very basic, non-interactive math service implemented in
// multiple ways:
// - function-based, dynamically typed
// - function-based, statically typed
// - state-based, dynamically typed
// - state-based, statically typed

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_actor.hpp"

using namespace caf;

// --(rst-calculator-actor-begin)--
struct calculator_trait {
  using signatures = type_list<result<int32_t>(add_atom, int32_t, int32_t),
                               result<int32_t>(sub_atom, int32_t, int32_t)>;
};

using calculator_actor = typed_actor<calculator_trait>;
// --(rst-calculator-actor-end)--

// --(rst-prototypes-begin)--
behavior calculator_fun();
calculator_actor::behavior_type typed_calculator_fun();
struct calculator_state;
struct typed_calculator_state;
// --(rst-prototypes-end)--

// --(rst-function-based-begin)--
// function-based, dynamically typed
behavior calculator_fun() {
  return {
    [](add_atom, int32_t a, int32_t b) { return a + b; },
    [](sub_atom, int32_t a, int32_t b) { return a - b; },
  };
}

// function-based, statically typed
calculator_actor::behavior_type typed_calculator_fun() {
  return {
    [](add_atom, int32_t a, int32_t b) { return a + b; },
    [](sub_atom, int32_t a, int32_t b) { return a - b; },
  };
}
// --(rst-function-based-end)--

// --(rst-state-based-begin)--
// state-based, dynamically typed
struct calculator_state {
  behavior make_behavior() {
    return {
      [](add_atom, int32_t a, int32_t b) { return a + b; },
      [](sub_atom, int32_t a, int32_t b) { return a - b; },
    };
  }
};

// state-based, statically typed
struct typed_calculator_state {
  calculator_actor::behavior_type make_behavior() {
    return {
      [](add_atom, int32_t a, int32_t b) { return a + b; },
      [](sub_atom, int32_t a, int32_t b) { return a - b; },
    };
  }
};
// --(rst-state-based-end)--

void tester(scoped_actor&) {
  // end of recursion
}

// tests a calculator instance
template <class Handle, class... Ts>
void tester(scoped_actor& self, const Handle& hdl, int32_t x, int32_t y,
            Ts&&... xs) {
  // test: x + y = z
  self->mail(add_atom_v, x, y)
    .request(hdl, infinite)
    .receive(
      [&self, x, y](int32_t z) { //
        self->println("{} + {} = {}", x, y, z);
      },
      [&self](const error& err) {
        self->println("AUT (actor under test) failed: {}", err);
      });
  tester(self, std::forward<Ts>(xs)...);
}

void caf_main(actor_system& sys) {
  // --(rst-spawn-begin)--
  auto a1 = sys.spawn(calculator_fun);
  auto a2 = sys.spawn(typed_calculator_fun);
  auto a3 = sys.spawn(actor_from_state<calculator_state>);
  auto a4 = sys.spawn(actor_from_state<typed_calculator_state>);
  // --(rst-spawn-end)--
  scoped_actor self{sys};
  tester(self, a1, 1, 2, a2, 3, 4, a3, 5, 6, a4, 7, 8);
}

CAF_MAIN()
