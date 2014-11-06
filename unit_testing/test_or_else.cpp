#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

void test_or_else() {
  scoped_actor self;
  message_handler handle_a {
    on("a") >> [] { return 1; }
  };
  message_handler handle_b {
    on("b") >> [] { return 2; }
  };
  message_handler handle_c {
    on("c") >> [] { return 3; }
  };
  auto run_testee([&](actor testee) {
    self->sync_send(testee, "a").await([](int i) {
      CAF_CHECK_EQUAL(i, 1);
    });
    self->sync_send(testee, "b").await([](int i) {
      CAF_CHECK_EQUAL(i, 2);
    });
    self->sync_send(testee, "c").await([](int i) {
      CAF_CHECK_EQUAL(i, 3);
    });
    self->send_exit(testee, exit_reason::user_shutdown);
    self->await_all_other_actors_done();

  });
  CAF_PRINT("run_testee: handle_a.or_else(handle_b).or_else(handle_c)");
  run_testee(
    spawn([=] {
      return handle_a.or_else(handle_b).or_else(handle_c);
    })
  );
  CAF_PRINT("run_testee: handle_a.or_else(handle_b), on(\"c\") ...");
  run_testee(
    spawn([=] {
      return (
        handle_a.or_else(handle_b),
        on("c") >> [] { return 3; }
      );
    })
  );
  CAF_PRINT("run_testee: on(\"a\") ..., handle_b.or_else(handle_c)");
  run_testee(
    spawn([=] {
      return (
        on("a") >> [] { return 1; },
        handle_b.or_else(handle_c)
      );
    })
  );
}

int main() {
  CAF_TEST(test_or_else);
  test_or_else();
  return CAF_TEST_RESULT();
}
