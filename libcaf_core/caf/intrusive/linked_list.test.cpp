// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/intrusive/linked_list.hpp"

#include "caf/test/test.hpp"

#include "caf/intrusive/singly_linked.hpp"

#include <memory>
#include <numeric>

using namespace caf;

namespace {

struct inode : caf::intrusive::singly_linked<inode> {
  explicit inode(int x = 0) : value(x) {
    // nop
  }

  int value;
};

std::string to_string(const inode& x) {
  return std::to_string(x.value);
}

using list_type = intrusive::linked_list<inode>;

template <class... Ts>
void fill(list_type& xs, Ts... args) {
  (xs.emplace_back(args), ...);
}

TEST("a default default-constructed list is empty") {
  list_type uut;
  check_eq(uut.empty(), true);
  check_eq(uut.size(), 0u);
  check_eq(uut.peek(), nullptr);
  check_eq(uut.begin(), uut.end());
}

TEST("lists are convertible to strings") {
  list_type uut;
  check_eq(deep_to_string(uut), "[]");
  fill(uut, 1, 2, 3, 4);
  check_eq(deep_to_string(uut), "[1, 2, 3, 4]");
}

TEST("push_back adds elements to the back of the list") {
  list_type uut;
  uut.emplace_back(1);
  uut.push_back(std::make_unique<inode>(2));
  uut.push_back(new inode(3));
  check_eq(deep_to_string(uut), "[1, 2, 3]");
}

TEST("push_front adds elements to the front of the list") {
  list_type uut;
  uut.emplace_front(1);
  uut.push_front(std::make_unique<inode>(2));
  uut.push_front(new inode(3));
  check_eq(deep_to_string(uut), "[3, 2, 1]");
}

TEST("insert_after inserts elements after a given position") {
  list_type uut;
  uut.insert_after(uut.before_end(), new inode(1));
  uut.insert_after(uut.before_end(), new inode(3));
  uut.insert_after(uut.begin(), new inode(2));
  uut.insert_after(uut.before_begin(), new inode(0));
  check_eq(deep_to_string(uut), "[0, 1, 2, 3]");
}

TEST("lists are movable") {
  list_type uut;
  SECTION("move constructor") {
    fill(uut, 1, 2, 3);
    list_type q2 = std::move(uut);
    check_eq(uut.empty(), true); // NOLINT(bugprone-use-after-move)
    check_eq(q2.empty(), false);
    check_eq(deep_to_string(q2), "[1, 2, 3]");
  }
  SECTION("move assignment operator") {
    list_type q2;
    fill(q2, 1, 2, 3);
    uut = std::move(q2);
    check_eq(q2.empty(), true); // NOLINT(bugprone-use-after-move)
    check_eq(uut.empty(), false);
    check_eq(deep_to_string(uut), "[1, 2, 3]");
  }
}

TEST("peek returns a pointer to the first element without removing it") {
  list_type uut;
  check_eq(uut.peek(), nullptr);
  fill(uut, 1, 2, 3);
  check_eq(uut.peek()->value, 1);
}

TEST("the size of the list is the number of elements") {
  list_type uut;
  fill(uut, 1, 2, 3);
  check_eq(uut.size(), 3u);
  fill(uut, 4, 5);
  check_eq(uut.size(), 5u);
}

TEST("calling clear removes all elements from a list") {
  list_type uut;
  fill(uut, 1, 2, 3);
  check_eq(uut.size(), 3u);
  uut.clear();
  check_eq(uut.size(), 0u);
}

TEST("find_if selects an element from the list") {
  list_type uut;
  fill(uut, 1, 2, 3);
  SECTION("find_if returns a pointer to the first matching element") {
    auto ptr = uut.find_if([](const inode& x) { return x.value == 2; });
    if (check(ptr != nullptr))
      check_eq(ptr->value, 2);
  }
  SECTION("find_if returns nullptr if nothing matches") {
    auto ptr = uut.find_if([](const inode& x) { return x.value == 4; });
    check(ptr == nullptr);
  }
}

TEST("lists allow iterator-based access") {
  list_type uut;
  fill(uut, 1, 2, 3);
  // Mutable access via begin/end.
  for (auto& x : uut)
    x.value *= 2;
  check_eq(uut.front()->value, 2);
  check_eq(uut.back()->value, 6);
  // Immutable access via cbegin/cend.
  check_eq(std::accumulate(uut.cbegin(), uut.cend(), 0,
                           [](int acc, const inode& x) {
                             return acc + x.value;
                           }),
           12);
}

TEST("pop_front removes the oldest element of a list and returns it") {
  list_type uut;
  fill(uut, 1, 2, 3);
  check_eq(uut.pop_front()->value, 1);
  if (check_eq(uut.size(), 2u))
    check_eq(uut.pop_front()->value, 2);
  if (check_eq(uut.size(), 1u))
    check_eq(uut.pop_front()->value, 3);
  check(uut.empty());
}

} // namespace
