// This example illustrates how to use the `skip` default handler to keep
// messages in the mailbox until an actor switches to another behavior that can
// handle them.

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/fwd.hpp"
#include "caf/mail_cache.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

struct server_state {
  server_state(event_based_actor* selfptr) : self(selfptr), cache(self, 10) {
    // nop
  }

  behavior make_behavior() {
    return {
      [this](idle_atom, const actor& worker) {
        // Passing keep_behavior allows us to return to this behavior later on
        // by calling `self->unbecome()`.
        self->become(keep_behavior, [this, worker](ping_atom) {
          // Switch back to our default behavior, put all messages from the
          // cache back to the mailbox and delegate 'ping' to the worker.
          self->unbecome();
          cache.unstash();
          return self->mail(ping_atom_v).delegate(worker);
        });
      },
      [this](message msg) {
        // Stash all messages until we receive an `idle_atom`.
        cache.stash(std::move(msg));
      },
    };
  }

  event_based_actor* self;
  mail_cache cache;
};

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
  auto serv = sys.spawn(actor_from_state<server_state>);
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
