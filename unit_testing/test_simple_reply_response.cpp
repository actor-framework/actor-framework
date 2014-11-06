#include "test.hpp"

#include "caf/all.hpp"

using namespace caf;

void test_simple_reply_response() {
  auto s = spawn([](event_based_actor* self) -> behavior {
    return (
      others() >> [=]() -> message {
        CAF_CHECK(self->last_dequeued() == make_message(atom("hello")));
        self->quit();
        return self->last_dequeued();
      }
    );
  });
  scoped_actor self;
  self->send(s, atom("hello"));
  self->receive(
    others() >> [&] {
      CAF_CHECK(self->last_dequeued() == make_message(atom("hello")));
    }
  );
  self->await_all_other_actors_done();
}

int main() {
  CAF_TEST(test_simple_reply_response);
  test_simple_reply_response();
  return CAF_TEST_RESULT();
}
