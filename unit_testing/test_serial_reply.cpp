#include <iostream>

#include "test.hpp"

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

void test_serial_reply() {
  auto mirror_behavior = [=](event_based_actor* self) {
    self->become(others() >> [=]() -> message {
      CAF_PRINT("return self->last_dequeued()");
      return self->last_dequeued();
    });
  };
  auto master = spawn([=](event_based_actor* self) {
    cout << "ID of master: " << self->id() << endl;
    // spawn 5 mirror actors
    auto c0 = self->spawn<linked>(mirror_behavior);
    auto c1 = self->spawn<linked>(mirror_behavior);
    auto c2 = self->spawn<linked>(mirror_behavior);
    auto c3 = self->spawn<linked>(mirror_behavior);
    auto c4 = self->spawn<linked>(mirror_behavior);
    self->become (
      on(atom("hi there")) >> [=]() -> continue_helper {
      CAF_PRINT("received 'hi there'");
      return self->sync_send(c0, atom("sub0")).then(
        on(atom("sub0")) >> [=]() -> continue_helper {
        CAF_PRINT("received 'sub0'");
        return self->sync_send(c1, atom("sub1")).then(
          on(atom("sub1")) >> [=]() -> continue_helper {
          CAF_PRINT("received 'sub1'");
          return self->sync_send(c2, atom("sub2")).then(
            on(atom("sub2")) >> [=]() -> continue_helper {
            CAF_PRINT("received 'sub2'");
            return self->sync_send(c3, atom("sub3")).then(
              on(atom("sub3")) >> [=]() -> continue_helper {
              CAF_PRINT("received 'sub3'");
              return self->sync_send(c4, atom("sub4")).then(
                on(atom("sub4")) >> [=]() -> atom_value {
                CAF_PRINT("received 'sub4'");
                return atom("hiho");
                }
              );
              }
            );
            }
          );
          }
        );
        }
      );
      }
    );
    }
  );
  { // lifetime scope of self
    scoped_actor self;
    cout << "ID of main: " << self->id() << endl;
    self->sync_send(master, atom("hi there")).await(
      on(atom("hiho")) >> [] {
        CAF_CHECKPOINT();
      },
      others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
    );
    self->send_exit(master, exit_reason::user_shutdown);
  }
  await_all_actors_done();
}

int main() {
  CAF_TEST(test_serial_reply);
  test_serial_reply();
  return CAF_TEST_RESULT();
}
