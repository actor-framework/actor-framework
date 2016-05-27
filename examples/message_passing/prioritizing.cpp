#include "caf/all.hpp"

using std::endl;
using namespace caf;

behavior foo(event_based_actor* self) {
  self->send(self, "world");
  self->send<message_priority::high>(self, "hello");
  // when spawning `foo` with priority_aware flag, it will print "hello" first
  return {
    [=](const std::string& str) {
      aout(self) << str << endl;
    }
  };
}

void caf_main(actor_system& system) {
  scoped_actor self{system};
  aout(self) << "spawn foo" << endl;
  self->spawn(foo);
  self->await_all_other_actors_done();
  aout(self) << "spawn foo again with priority_aware flag" << endl;
  self->spawn<priority_aware>(foo);
}

CAF_MAIN()
