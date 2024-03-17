// This example illustrates how to use the `skip` default handler to keep
// messages in the mailbox until an actor switches to another behavior that can
// handle them.

#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

behavior server(event_based_actor* self) {
  // Setting the default handler to `skip` will keep all unmatched messages in
  // the mailbox until we switch to another behavior that handles them. This
  // allows us to receive idle and ping messages in any order but process them
  // in alternating order.
  self->set_default_handler(skip);
  return {
    [self](idle_atom, const actor& worker) {
      // Passing keep_behavior allows us to return to this behavior later on by
      // calling `self->unbecome()`.
      self->become(keep_behavior, [self, worker](ping_atom atm) {
        self->delegate(worker, atm);
        self->unbecome();
      });
    },
  };
}

behavior client(event_based_actor* self, const actor& serv) {
  self->link_to(serv);
  self->mail(idle_atom_v, self).send(serv);
  return {
    [self, serv](ping_atom) {
      self->mail(idle_atom_v, self).send(serv);
      return pong_atom_v;
    },
  };
}

void caf_main(actor_system& sys) {
  auto serv = sys.spawn(server);
  auto worker = sys.spawn(client, serv);
  scoped_actor self{sys};
  self->mail(ping_atom_v)
    .request(serv, 10s)
    .receive(
      [&self, worker](pong_atom) {
        self->println("received response from {}",
                      self->current_sender() == worker ? "worker" : "server");
      },
      [&self, worker](error& err) {
        self->println("received error {} from {}", err,
                      self->current_sender() == worker ? "worker" : "server");
      });
  self->send_exit(serv, exit_reason::user_shutdown);
}

CAF_MAIN()
