#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

void test_simple_reply_response() {
  auto s = spawn([](event_based_actor* self) -> behavior {
    return (
      others >> [=]() -> message {
        CAF_CHECK(self->current_message() == make_message(ok_atom::value));
        self->quit();
        return self->current_message();
      }
    );
  });
  scoped_actor self;
  self->send(s, ok_atom::value);
  self->receive(
    others >> [&] {
      CAF_CHECK(self->current_message() == make_message(ok_atom::value));
    }
  );
  self->await_all_other_actors_done();
}

int main() {
  CAF_TEST(test_simple_reply_response);
  test_simple_reply_response();
  shutdown();
  return CAF_TEST_RESULT();
}
