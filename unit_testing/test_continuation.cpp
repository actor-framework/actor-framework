#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

behavior simple_mirror(event_based_actor* self) {
  return {
    others() >> [=] {
      return self->last_dequeued();
    }
  };
}

void test_continuation() {
  auto mirror = spawn(simple_mirror);
  spawn([=](event_based_actor* self) {
    self->sync_send(mirror, 42).then(
      on(42) >> [] {
        return "fourty-two";
      }
    ).continue_with(
      [=](const std::string& ref) {
        CAF_CHECK_EQUAL(ref, "fourty-two");
        return 4.2f;
      }
    ).continue_with(
      [=](float f) {
        CAF_CHECK_EQUAL(f, 4.2f);
        self->send_exit(mirror, exit_reason::user_shutdown);
        self->quit();
      }
    );
  });
  await_all_actors_done();
}

int main() {
  CAF_TEST(test_continuation);
  test_continuation();
  return CAF_TEST_RESULT();
}
