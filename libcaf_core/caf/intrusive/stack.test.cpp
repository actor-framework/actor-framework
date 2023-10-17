// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/intrusive/stack.hpp"

#include "caf/test/test.hpp"

#include "caf/intrusive/singly_linked.hpp"

using namespace caf;

namespace {

struct int_node : intrusive::singly_linked<int_node> {
  explicit int_node(int x) : value(x) {
    // nop
  }

  int value = 0;
};

using int_stack = intrusive::stack<int_node>;

void push(int_stack& xs, int x) {
  xs.push(new int_node(x));
}

auto pop(int_stack& xs) {
  if (xs.empty())
    CAF_RAISE_ERROR("cannot pop from an empty stack");
  auto ptr = std::unique_ptr<int_node>{xs.pop()};
  return ptr->value;
}

TEST("a default-constructed stack is empty") {
  int_stack uut;
  check(uut.empty());
  check_eq(uut.pop(), nullptr);
}

TEST("pushing values to a stack makes it non-empty") {
  int_stack uut;
  check(uut.empty());
  push(uut, 1);
  check(!uut.empty());
  push(uut, 2);
  check(!uut.empty());
  push(uut, 3);
  check(!uut.empty());
}

TEST("popping values from a stack returns the last pushed value") {
  int_stack uut;
  check(uut.empty());
  push(uut, 1);
  push(uut, 2);
  push(uut, 3);
  check(!uut.empty());
  check_eq(pop(uut), 3);
  check_eq(pop(uut), 2);
  check_eq(pop(uut), 1);
  check(uut.empty());
}

} // namespace
