// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/intrusive/arc.hpp"

#include "caf/test/test.hpp"

#include <memory>
#include <type_traits>
#include <utility>

using namespace caf;
using namespace caf::intrusive;

namespace {

struct object {
  explicit object(std::shared_ptr<bool> destroyed, int value = 0)
    : destroyed(std::move(destroyed)), value(value) {
    // nop
  }

  virtual ~object() {
    *destroyed = true;
  }

  std::shared_ptr<bool> destroyed;
  int value;
};

struct derived : object {
  using object::object;
};

} // namespace

TEST("default arc pointers are empty") {
  arc_ptr<object> strong;
  arc_weak_ptr<object> weak;
  check(!strong);
  check_eq(strong.storage(), nullptr);
  check(!weak);
  check_eq(weak.storage(), nullptr);
  check(weak.expired());
  check(!weak.upgrade());
  check(!strong.downgrade());
}

TEST("make_arc constructs managed objects") {
  auto destroyed = std::make_shared<bool>(false);
  {
    auto ptr = make_arc<object>(destroyed, 42);
    require_ne(ptr.storage(), nullptr);
    check(!*destroyed);
    check_eq(ptr->value, 42);
    check_eq((*ptr).value, 42);
    check_eq(ptr.strong_count(), 1u);
    check_eq(ptr.weak_count(), 1u);
  }
  check(*destroyed);
}

TEST("strong pointers copy, move and replace ownership") {
  auto destroyed = std::make_shared<bool>(false);
  {
    auto first = make_arc<object>(destroyed, 1);
    require_ne(first.storage(), nullptr);
    auto copy = first;
    require_ne(copy.storage(), nullptr);
    check_eq(first.storage(), copy.storage());
    check_eq(first.strong_count(), 2u);
    auto moved = std::move(copy);
    check(!copy);
    require_ne(moved.storage(), nullptr);
    check_eq(moved.strong_count(), 2u);
    auto other_destroyed = std::make_shared<bool>(false);
    auto other = make_arc<object>(other_destroyed, 2);
    other = first;
    require_ne(other.storage(), nullptr);
    check(*other_destroyed);
    check(!*destroyed);
    check_eq(first.strong_count(), 3u);
    auto& self = other;
    other = self;
    other = std::move(self);
    check_eq(other.strong_count(), 3u);
    copy = std::move(other);
    check(!other);
    require_ne(copy.storage(), nullptr);
    check_eq(copy.strong_count(), 3u);
    copy = arc_ptr<object>{};
    check(!copy);
    check_eq(first.strong_count(), 2u);
  }
  check(*destroyed);
}

TEST("weak pointers track object lifetime and upgrade") {
  auto destroyed = std::make_shared<bool>(false);
  arc_weak_ptr<object> weak;
  {
    auto strong = make_arc<object>(destroyed, 7);
    require_ne(strong.storage(), nullptr);
    weak = strong.downgrade();
    require_ne(weak.storage(), nullptr);
    check(!weak.expired());
    check_eq(weak.storage(), strong.storage());
    check_eq(weak.strong_count(), 1u);
    check_eq(weak.weak_count(), 2u);
    auto upgraded = weak.upgrade();
    require_ne(upgraded.storage(), nullptr);
    check_eq(upgraded->value, 7);
    check_eq(strong.strong_count(), 2u);
  }
  check(*destroyed);
  require_ne(weak.storage(), nullptr);
  check(weak.expired());
  check_eq(weak.strong_count(), 0u);
  check_eq(weak.weak_count(), 1u);
  check(!weak.upgrade());
}

TEST("weak pointers copy, move and replace ownership") {
  auto destroyed = std::make_shared<bool>(false);
  auto first = make_arc<object>(destroyed);
  require_ne(first.storage(), nullptr);
  auto other_destroyed = std::make_shared<bool>(false);
  auto other = make_arc<object>(other_destroyed);
  require_ne(other.storage(), nullptr);
  auto weak = first.downgrade();
  require_ne(weak.storage(), nullptr);
  auto copy = weak;
  check_eq(weak.weak_count(), 3u);
  auto moved = std::move(copy);
  check(!copy);
  require_ne(moved.storage(), nullptr);
  check_eq(moved.weak_count(), 3u);
  copy = other.downgrade();
  copy = weak;
  check_eq(other.weak_count(), 1u);
  check_eq(weak.weak_count(), 4u);
  auto& self = copy;
  copy = self;
  copy = std::move(self);
  check_eq(copy.weak_count(), 4u);
  copy = std::move(moved);
  check(!moved);
  require_ne(copy.storage(), nullptr);
  check_eq(copy.weak_count(), 3u);
  copy = arc_weak_ptr<object>{};
  check(!copy);
  check_eq(weak.weak_count(), 2u);
}

TEST("strong pointers convert from derived types") {
  auto destroyed = std::make_shared<bool>(false);
  {
    auto ptr = make_arc<derived>(destroyed, 42);
    require_ne(ptr.storage(), nullptr);
    arc_ptr<object> base = ptr;
    require_ne(base.storage(), nullptr);
    check_eq(base.storage(), ptr.storage());
    check_eq(base->value, 42);
    check_eq(base.strong_count(), 2u);
    arc_ptr<object> assigned;
    assigned = ptr;
    require_ne(assigned.storage(), nullptr);
    check_eq(assigned.strong_count(), 3u);
    arc_ptr<object> moved = std::move(ptr);
    check(!ptr);
    require_ne(moved.storage(), nullptr);
    check_eq(moved.storage(), base.storage());
    check_eq(moved.strong_count(), 3u);
    auto another_destroyed = std::make_shared<bool>(false);
    auto another = make_arc<derived>(another_destroyed, 9);
    assigned = std::move(another);
    check(!another);
    require_ne(assigned.storage(), nullptr);
    check_eq(assigned->value, 9);
  }
  check(*destroyed);
}
