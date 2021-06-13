// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.unique_function

#include "caf/detail/unique_function.hpp"

#include "caf/test/dsl.hpp"

namespace {

using int_fun = caf::detail::unique_function<int()>;

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

} // namespace

#define CHECK_VALID(f)                                                         \
  CAF_CHECK(!f.is_nullptr());                                                  \
  CAF_CHECK(f);                                                                \
  CAF_CHECK(f != nullptr);                                                     \
  CAF_CHECK(nullptr != f);                                                     \
  CAF_CHECK(!(f == nullptr));                                                  \
  CAF_CHECK(!(nullptr == f));                                                  \
  CAF_CHECK(f() == 42)

#define CHECK_INVALID(f)                                                       \
  CAF_CHECK(f.is_nullptr());                                                   \
  CAF_CHECK(!f);                                                               \
  CAF_CHECK(f == nullptr);                                                     \
  CAF_CHECK(nullptr == f);                                                     \
  CAF_CHECK(!(f != nullptr));                                                  \
  CAF_CHECK(!(nullptr != f));                                                  \
  CAF_CHECK(!f.holds_wrapper())

CAF_TEST(default construction) {
  int_fun f;
  CHECK_INVALID(f);
}

CAF_TEST(raw function pointer construction) {
  int_fun f{forty_two};
  CHECK_VALID(f);
}

CAF_TEST(stateless lambda construction) {
  int_fun f{[] { return 42; }};
  CHECK_VALID(f);
  CAF_CHECK(!f.holds_wrapper());
}

CAF_TEST(stateful lambda construction) {
  int i = 42;
  int_fun f{[=] { return i; }};
  CHECK_VALID(f);
  CAF_CHECK(f.holds_wrapper());
}

CAF_TEST(custom wrapper construction) {
  size_t instances = 0;
  { // lifetime scope of our counting wrapper
    int_fun f{new instance_counting_wrapper(&instances)};
    CHECK_VALID(f);
    CAF_CHECK(f.holds_wrapper());
    CAF_CHECK(instances == 1);
  }
  CAF_CHECK(instances == 0);
}

CAF_TEST(function move construction) {
  int_fun f{forty_two};
  int_fun g{std::move(f)};
  CHECK_INVALID(f);
  CHECK_VALID(g);
  CAF_CHECK(!g.holds_wrapper());
}

CAF_TEST(stateful lambda move construction) {
  int i = 42;
  int_fun f{[=] { return i; }};
  int_fun g{std::move(f)};
  CHECK_INVALID(f);
  CHECK_VALID(g);
  CAF_CHECK(g.holds_wrapper());
}

CAF_TEST(custom wrapper move construction) {
  size_t instances = 0;
  { // lifetime scope of our counting wrapper
    int_fun f{new instance_counting_wrapper(&instances)};
    int_fun g{std::move(f)};
    CHECK_INVALID(f);
    CHECK_VALID(g);
    CAF_CHECK(g.holds_wrapper());
    CAF_CHECK(instances == 1);
  }
  CAF_CHECK(instances == 0);
}

CAF_TEST(function assign) {
  size_t instances = 0;
  int_fun f;
  int_fun g{forty_two};
  int_fun h{new instance_counting_wrapper(&instances)};
  CAF_CHECK(instances == 1);
  CHECK_INVALID(f);
  CHECK_VALID(g);
  CHECK_VALID(h);
  f = forty_two;
  g = forty_two;
  h = forty_two;
  CAF_CHECK(instances == 0);
  CHECK_VALID(f);
  CHECK_VALID(g);
  CHECK_VALID(h);
}

CAF_TEST(move assign) {
  size_t instances = 0;
  int_fun f;
  int_fun g{forty_two};
  int_fun h{new instance_counting_wrapper(&instances)};
  CAF_CHECK(instances == 1);
  CHECK_INVALID(f);
  CHECK_VALID(g);
  CHECK_VALID(h);
  g = std::move(h);
  CAF_CHECK(instances == 1);
  CHECK_INVALID(f);
  CHECK_VALID(g);
  CHECK_INVALID(h);
  f = std::move(g);
  CAF_CHECK(instances == 1);
  CHECK_VALID(f);
  CHECK_INVALID(g);
  CHECK_INVALID(h);
  f = int_fun{};
  CAF_CHECK(instances == 0);
  CHECK_INVALID(f);
  CHECK_INVALID(g);
  CHECK_INVALID(h);
}
