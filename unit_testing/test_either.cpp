#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

using foo = typed_actor<replies_to<int>::with_either<int>::or_else<float>>;

foo::behavior_type my_foo(foo::pointer self) {
  return {
    [](int arg) -> either<int>::or_else<float> {
      if (arg == 42) {
        return 42;
      }
      return static_cast<float>(arg);
    }
  };
}

void test_either() {
  auto f1 = []() -> either<int>::or_else<float> {
    return 42;
  };
  auto f2 = []() -> either<int>::or_else<float> {
    return 42.f;
  };
  auto f3 = [](bool flag) -> either<int, int>::or_else<float, float> {
    if (flag) {
      return {1, 2};
    }
    return {3.f, 4.f};
  };
  f1();
  f2();
  f3(true);
  f3(false);
  either<int>::or_else<float> x1{4};
  either<int>::or_else<float> x2{4.f};
  auto mf = spawn_typed(my_foo);
  scoped_actor self;
  self->sync_send(mf, 42).await(
    [](int val) {
      CAF_CHECK_EQUAL(val, 42);
    },
    [](float) {
      CAF_FAILURE("expected an integer");
    }
  );
  self->sync_send(mf, 10).await(
    [](int) {
    CAF_FAILURE("expected a float");
    },
    [](float val) {
      CAF_CHECK_EQUAL(val, 10.f);
    }
  );
}

int main() {
  CAF_TEST(test_either);
  test_either();
  return CAF_TEST_RESULT();
}
