#include "caf/all.hpp"

using namespace caf;

behavior server(event_based_actor* self) {
  self->set_default_handler(skip);
  return {
    [=](idle_atom, const actor& worker) {
      self->become(keep_behavior, [=](ping_atom atm) {
        self->delegate(worker, atm);
        self->unbecome();
      });
    },
  };
}

behavior client(event_based_actor* self, const actor& serv) {
  self->link_to(serv);
  self->send(serv, idle_atom_v, self);
  return {
    [=](ping_atom) {
      self->send(serv, idle_atom_v, self);
      return pong_atom_v;
    },
  };
}

void caf_main(actor_system& system) {
  auto serv = system.spawn(server);
  auto worker = system.spawn(client, serv);
  scoped_actor self{system};
  self->request(serv, std::chrono::seconds(10), ping_atom_v)
    .receive(
      [&](pong_atom) {
        aout(self) << "received response from "
                   << (self->current_sender() == worker ? "worker\n"
                                                        : "server\n");
      },
      [&](error& err) {
        aout(self) << "received error " << to_string(err) << " from "
                   << (self->current_sender() == worker ? "worker\n"
                                                        : "server\n");
      });
  self->send_exit(serv, exit_reason::user_shutdown);
}

CAF_MAIN()
