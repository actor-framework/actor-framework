// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/intrusive/lifo_uptr_queue.hpp"

#include "caf/test/test.hpp"

#include <string>

using namespace caf;

using namespace std::literals;

namespace {

struct int_node {
  explicit int_node(int x) : value(x) {
    // nop
  }

  int value = 0;
  std::unique_ptr<int_node> next;
};

using int_queue = intrusive::lifo_uptr_queue<int_node>;

void push(int_queue& xs, int x) {
  xs.push(std::make_unique<int_node>(x));
}

auto pop(int_queue& xs) {
  if (xs.empty())
    CAF_RAISE_ERROR("cannot pop from an empty stack");
  return xs.pop()->value;
}

auto stack_to_string(const int_queue& xs) {
  if (xs.empty()) {
    return "[]"s;
  }
  std::string result;
  result = '[';
  auto* current = xs.top().get();
  detail::format_to(std::back_inserter(result), "{}", current->value);
  current = current->next.get();
  while (current) {
    detail::format_to(std::back_inserter(result), ", {}", current->value);
    current = current->next.get();
  }
  result += ']';
  return result;
}

} // namespace

TEST("a default-constructed stack is empty") {
  int_queue uut;
  check(uut.empty());
  check_eq(uut.pop(), nullptr);
  check_eq(stack_to_string(uut), "[]");
}

TEST("pushing values to a stack makes it non-empty") {
  int_queue uut;
  check(uut.empty());
  push(uut, 1);
  check(!uut.empty());
  push(uut, 2);
  check(!uut.empty());
  push(uut, 3);
  check(!uut.empty());
  check_eq(stack_to_string(uut), "[3, 2, 1]");
}

TEST("popping values from a stack returns the last pushed value") {
  int_queue uut;
  check(uut.empty());
  push(uut, 1);
  push(uut, 2);
  push(uut, 3);
  check(!uut.empty());
  check_eq(pop(uut), 3);
  check_eq(pop(uut), 2);
  check_eq(pop(uut), 1);
  check(uut.empty());
  check_eq(stack_to_string(uut), "[]");
}

TEST("erase_if removes all elements that match the predicate") {
  int_queue uut;
  push(uut, 1);
  push(uut, 2);
  push(uut, 3);
  SECTION("erase the first element") {
    auto pred = [](const int_node& node) { return node.value == 3; };
    check(uut.erase_if(pred));
    check_eq(stack_to_string(uut), "[2, 1]");
  }
  SECTION("erase the first two elements") {
    auto pred = [](const int_node& node) { return node.value > 1; };
    check(uut.erase_if(pred));
    check_eq(stack_to_string(uut), "[1]");
  }
  SECTION("erase the all elements") {
    auto pred = [](const int_node&) { return true; };
    check(uut.erase_if(pred));
    check_eq(stack_to_string(uut), "[]");
  }
  SECTION("erase the middle element") {
    auto pred = [](const int_node& node) { return node.value == 2; };
    check(uut.erase_if(pred));
    check_eq(stack_to_string(uut), "[3, 1]");
  }
  SECTION("erase the middle and last element") {
    auto pred = [](const int_node& node) { return node.value <= 2; };
    check(uut.erase_if(pred));
    check_eq(stack_to_string(uut), "[3]");
  }
  SECTION("erase the last element") {
    auto pred = [](const int_node& node) { return node.value == 1; };
    check(uut.erase_if(pred));
    check_eq(stack_to_string(uut), "[3, 2]");
  }
  SECTION("erase nothing") {
    auto pred = [](const int_node&) { return false; };
    check(!uut.erase_if(pred));
    check_eq(stack_to_string(uut), "[3, 2, 1]");
  }
}

TEST("erase_first_if removes the first element that matches the predicate") {
  int_queue uut;
  push(uut, 1);
  push(uut, 2);
  push(uut, 3);
  SECTION("erase the first element") {
    auto pred = [](const int_node&) { return true; };
    check(uut.erase_first_if(pred));
    check_eq(stack_to_string(uut), "[2, 1]");
  }
  SECTION("erase the middle element") {
    auto pred = [](const int_node& node) { return node.value <= 2; };
    check(uut.erase_first_if(pred));
    check_eq(stack_to_string(uut), "[3, 1]");
  }
  SECTION("erase the last element") {
    auto pred = [](const int_node& node) { return node.value <= 1; };
    check(uut.erase_first_if(pred));
    check_eq(stack_to_string(uut), "[3, 2]");
  }
  SECTION("erase nothing") {
    auto pred = [](const int_node&) { return false; };
    check(!uut.erase_first_if(pred));
    check_eq(stack_to_string(uut), "[3, 2, 1]");
  }
}

TEST("any_of checks whether any element matches the predicate") {
  int_queue uut;
  SECTION("empty queue") {
    check(!uut.any_of([](const int_node&) { return true; }));
  }
  SECTION("single element matching") {
    push(uut, 7);
    check(uut.any_of([](const int_node& n) { return n.value == 7; }));
  }
  SECTION("single element not matching") {
    push(uut, 7);
    check(!uut.any_of([](const int_node& n) { return n.value == 0; }));
  }
  SECTION("matches the head element") {
    push(uut, 1);
    push(uut, 2);
    push(uut, 3);
    check(uut.any_of([](const int_node& n) { return n.value == 3; }));
  }
  SECTION("matches a middle element") {
    push(uut, 1);
    push(uut, 2);
    push(uut, 3);
    check(uut.any_of([](const int_node& n) { return n.value == 2; }));
  }
  SECTION("matches the tail element") {
    push(uut, 1);
    push(uut, 2);
    push(uut, 3);
    check(uut.any_of([](const int_node& n) { return n.value == 1; }));
  }
  SECTION("no element matches") {
    push(uut, 1);
    push(uut, 2);
    push(uut, 3);
    check(!uut.any_of([](const int_node& n) { return n.value == 0; }));
  }
}

TEST("for_each applies the function to each element from head to tail") {
  int_queue uut;
  SECTION("empty queue") {
    std::string out;
    uut.for_each([&out](const int_node& n) { out += std::to_string(n.value); });
    check_eq(out, "");
  }
  SECTION("single element") {
    push(uut, 0);
    std::string out;
    uut.for_each([&out](const int_node& n) { out += std::to_string(n.value); });
    check_eq(out, "0");
  }
  SECTION("multiple elements") {
    push(uut, 1);
    push(uut, 2);
    push(uut, 3);
    std::string out;
    uut.for_each([&out](const int_node& n) { out += std::to_string(n.value); });
    check_eq(out, "321");
  }
}
