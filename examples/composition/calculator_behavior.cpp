/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * using composable states.                                                   *
\******************************************************************************/

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 20-52 (Actor.tex)

#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

namespace {

// using add_atom = atom_constant<atom("add")>; (defined in atom.hpp)
using multiply_atom = atom_constant<atom("multiply")>;

using adder = typed_actor<replies_to<add_atom, int, int>::with<int>>;
using multiplier = typed_actor<replies_to<multiply_atom, int, int>::with<int>>;

class adder_bhvr : public composable_behavior<adder> {
public:
  result<int> operator()(add_atom, int x, int y) override {
    return x + y;
  }
};

class multiplier_bhvr : public composable_behavior<multiplier> {
public:
  result<int> operator()(multiply_atom, int x, int y) override {
    return x * y;
  }
};

// calculator_bhvr can be inherited from or composed further
using calculator_bhvr = composed_behavior<adder_bhvr, multiplier_bhvr>;

} // namespace

void caf_main(actor_system& system) {
  auto f = make_function_view(system.spawn<calculator_bhvr>());
  cout << "10 + 20 = " << f(add_atom::value, 10, 20) << endl;
  cout << "7 * 9 = " << f(multiply_atom::value, 7, 9) << endl;
}

CAF_MAIN()
