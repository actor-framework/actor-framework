#include "caf/all.hpp"
using namespace caf;

using idle_atom = atom_constant<atom("idle")>;
using request_atom = atom_constant<atom("request")>;
using response_atom = atom_constant<atom("response")>;

behavior server(event_based_actor* self) {
  return {
    [=](idle_atom, const actor& worker) {
      self->become (
        keep_behavior,
        [=](request_atom atm) {
          self->delegate(worker, atm);
          self->unbecome();
        },
        [=](idle_atom) {
          return skip();
        }
      );
    },
    [=](request_atom) {
      return skip();
    }
  };
}

behavior client(event_based_actor* self, const actor& serv) {
  self->link_to(serv);
  self->send(serv, idle_atom::value, self);
  return {
    [=](request_atom) {
      self->send(serv, idle_atom::value, self);
      return response_atom::value;
    }
  };
}

void caf_main(actor_system& system) {
  auto serv = system.spawn(server);
  auto worker = system.spawn(client, serv);
  scoped_actor self{system};
  self->request(serv, std::chrono::seconds(10), request_atom::value).receive(
    [&](response_atom) {
      aout(self) << "received response from "
                 << (self->current_sender() == worker ? "worker\n"
                                                      : "server\n");
    },
    [&](error& err) {
      aout(self) << "received error "
                 << system.render(err)
                 << " from "
                 << (self->current_sender() == worker ? "worker\n"
                                                      : "server\n");
    }
  );
  self->send_exit(serv, exit_reason::user_shutdown);
}

CAF_MAIN()
