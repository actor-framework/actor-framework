#include <iostream>

#include "test.hpp"
#include "ping_pong.hpp"

#include "caf/all.hpp"
#include "caf/detail/logging.hpp"

using std::cout;
using std::endl;
using namespace caf;

namespace {

size_t s_pongs = 0;

behavior ping_behavior(local_actor* self, size_t num_pings) {
  return (on(atom("pong"), arg_match) >> [=](int value)->message {
        if (!self->last_sender()) {
          CAF_PRINT("last_sender() invalid!");
        }
        CAF_PRINT("received {'pong', " << value << "}");
        // cout << to_string(self->last_dequeued()) << endl;
        if (++s_pongs >= num_pings) {
          CAF_PRINT("reached maximum, send {'EXIT', user_defined} "
                 << "to last sender and quit with normal reason");
          self->send_exit(self->last_sender(),
                  exit_reason::user_shutdown);
          self->quit();
        }
        return make_message(atom("ping"), value);
      },
      others() >> [=] {
    CAF_LOGF_ERROR("unexpected message; "
            << to_string(self->last_dequeued()));
    self->quit(exit_reason::user_shutdown);
  });
}

behavior pong_behavior(local_actor* self) {
  return (on(atom("ping"), arg_match) >> [](int value)->message {
        CAF_PRINT("received {'ping', " << value << "}");
        return make_message(atom("pong"), value + 1);
      },
      others() >> [=] {
    CAF_LOGF_ERROR("unexpected message; "
            << to_string(self->last_dequeued()));
    self->quit(exit_reason::user_shutdown);
  });
}

} // namespace <anonymous>

size_t pongs() { return s_pongs; }

void ping(blocking_actor* self, size_t num_pings) {
  CAF_LOGF_TRACE("num_pings = " << num_pings);
  s_pongs = 0;
  self->receive_loop(ping_behavior(self, num_pings));
}

void event_based_ping(event_based_actor* self, size_t num_pings) {
  CAF_LOGF_TRACE("num_pings = " << num_pings);
  s_pongs = 0;
  self->become(ping_behavior(self, num_pings));
}

void pong(blocking_actor* self, actor ping_actor) {
  CAF_LOGF_TRACE("ping_actor = " << to_string(ping_actor));
  self->send(ping_actor, atom("pong"), 0); // kickoff
  self->receive_loop(pong_behavior(self));
}

void event_based_pong(event_based_actor* self, actor ping_actor) {
  CAF_LOGF_TRACE("ping_actor = " << to_string(ping_actor));
  CAF_REQUIRE(ping_actor != invalid_actor);
  self->send(ping_actor, atom("pong"), 0); // kickoff
  self->become(pong_behavior(self));
}
