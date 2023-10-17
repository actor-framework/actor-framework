// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/unique_function.hpp"

#include "caf/test/test.hpp"

using int_fun = caf::detail::unique_function<int()>;

namespace {

int forty_two() {
  return 42;
}

class instance_counting_wrapper final : public int_fun::wrapper {
public:
  instance_counting_wrapper(size_t* instance_counter)
    : instance_counter_(instance_counter) {
    *instance_counter_ += 1;
  }

  ~instance_counting_wrapper() {
    *instance_counter_ -= 1;
  }

  int operator()() final {
    return 42;
  }

private:
  size_t* instance_counter_;
};

#define CHECK_VALID(f)                                                         \
  check(!f.is_nullptr());                                                      \
  check(static_cast<bool>(f));                                                 \
  check_ne(f, nullptr);                                                        \
  check_ne(nullptr, f);                                                        \
  check(!(f == nullptr));                                                      \
  check(!(nullptr == f));                                                      \
  check_eq(f(), 42)

#define CHECK_INVALID(f)                                                       \
  check(f.is_nullptr());                                                       \
  check(!f);                                                                   \
  check_eq(f, nullptr);                                                        \
  check_eq(nullptr, f);                                                        \
  check(!(f != nullptr));                                                      \
  check(!(nullptr != f));                                                      \
  check(!f.holds_wrapper())

TEST("default construction") {
  int_fun f;
  CHECK_INVALID(f);
}

TEST("raw function pointer construction") {
  int_fun f{forty_two};
  CHECK_VALID(f);
}

TEST("stateless lambda construction") {
  int_fun f{[] { return 42; }};
  CHECK_VALID(f);
  check(!f.holds_wrapper());
}

TEST("stateful lambda construction") {
  int i = 42;
  int_fun f{[=] { return i; }};
  CHECK_VALID(f);
  check(f.holds_wrapper());
}

TEST("custom wrapper construction") {
  size_t instances = 0;
  { // lifetime scope of our counting wrapper
    int_fun f{new instance_counting_wrapper(&instances)};
    CHECK_VALID(f);
    check(f.holds_wrapper());
    check(instances == 1);
  }
  check(instances == 0);
}

// NOLINTBEGIN(bugprone-use-after-move)

TEST("function move construction") {
  int_fun f{forty_two};
  int_fun g{std::move(f)};
  CHECK_INVALID(f);
  CHECK_VALID(g);
  check(!g.holds_wrapper());
}

TEST("stateful lambda move construction") {
  int i = 42;
  int_fun f{[=] { return i; }};
  int_fun g{std::move(f)};
  CHECK_INVALID(f);
  CHECK_VALID(g);
  check(g.holds_wrapper());
}

TEST("custom wrapper move construction") {
  size_t instances = 0;
  { // lifetime scope of our counting wrapper
    int_fun f{new instance_counting_wrapper(&instances)};
    int_fun g{std::move(f)};
    CHECK_INVALID(f);
    CHECK_VALID(g);
    check(g.holds_wrapper());
    check(instances == 1);
  }
  check(instances == 0);
}

TEST("function assign") {
  size_t instances = 0;
  int_fun f;
  int_fun g{forty_two};
  int_fun h{new instance_counting_wrapper(&instances)};
  check(instances == 1);
  CHECK_INVALID(f);
  CHECK_VALID(g);
  CHECK_VALID(h);
  f = forty_two;
  g = forty_two;
  h = forty_two;
  check(instances == 0);
  CHECK_VALID(f);
  CHECK_VALID(g);
  CHECK_VALID(h);
}

TEST("move assign") {
  size_t instances = 0;
  int_fun f;
  int_fun g{forty_two};
  int_fun h{new instance_counting_wrapper(&instances)};
  check(instances == 1);
  CHECK_INVALID(f);
  CHECK_VALID(g);
  CHECK_VALID(h);
  g = std::move(h);
  check(instances == 1);
  CHECK_INVALID(f);
  CHECK_VALID(g);
  CHECK_INVALID(h);
  f = std::move(g);
  check(instances == 1);
  CHECK_VALID(f);
  CHECK_INVALID(g);
  CHECK_INVALID(h);
  f = int_fun{};
  check(instances == 0);
  CHECK_INVALID(f);
  CHECK_INVALID(g);
  CHECK_INVALID(h);
}

// NOLINTEND(bugprone-use-after-move)

} // namespace
