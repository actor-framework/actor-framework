#include <vector>
#include <string>
#include <limits>
#include <memory>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#ifdef CAF_WINDOWS
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

CAF_PUSH_WARNINGS
#include "pingpong.pb.h"
CAF_POP_WARNINGS

namespace {

using namespace std;
using namespace caf;
using namespace caf::io;

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;
using kickoff_atom = atom_constant<atom("kickoff")>;

// utility function to print an exit message with custom name
void print_on_exit(scheduled_actor* self, const std::string& name) {
  self->attach_functor([=](const error& reason) {
    aout(self) << name << " exited: " << self->home_system().render(reason)
               << endl;
  });
}

struct ping_state {
  size_t count = 0;
};

behavior ping(stateful_actor<ping_state>* self, size_t num_pings) {
  print_on_exit(self, "ping");
  return {
    [=](kickoff_atom, const actor& pong) {
      self->send(pong, ping_atom::value, 1);
      self->become (
        [=](pong_atom, int value) -> message {
          if (++(self->state.count) >= num_pings)
            self->quit();
          return make_message(ping_atom::value, value + 1);
        }
      );
    }
  };
}

behavior pong(event_based_actor* self) {
  print_on_exit(self, "pong");
  return {
    [=](ping_atom, int value) {
      return make_message(pong_atom::value, value);
    }
  };
}

void protobuf_io(broker* self, connection_handle hdl, const actor& buddy) {
  print_on_exit(self, "protobuf_io");
  aout(self) << "protobuf broker started" << endl;
  self->monitor(buddy);
  self->set_down_handler(
    [=](const down_msg& dm) {
      if (dm.source == buddy) {
        aout(self) << "our buddy is down" << endl;
        self->quit(dm.reason);
      }
    });
  auto write = [=](const org::libcppa::PingOrPong& p) {
    string buf = p.SerializeAsString();
    auto s = htonl(static_cast<uint32_t>(buf.size()));
    self->write(hdl, sizeof(uint32_t), &s);
    self->write(hdl, buf.size(), buf.data());
    self->flush(hdl);
  };
  message_handler default_bhvr = {
    [=](const connection_closed_msg&) {
      aout(self) << "connection closed" << endl;
      self->send_exit(buddy, exit_reason::remote_link_unreachable);
      self->quit(exit_reason::remote_link_unreachable);
    },
    [=](ping_atom, int i) {
      aout(self) << "'ping' " << i << endl;
      org::libcppa::PingOrPong p;
      p.mutable_ping()->set_id(i);
      write(p);
    },
    [=](pong_atom, int i) {
      aout(self) << "'pong' " << i << endl;
      org::libcppa::PingOrPong p;
      p.mutable_pong()->set_id(i);
      write(p);
    }
  };
  auto await_protobuf_data = message_handler {
    [=](const new_data_msg& msg) {
      org::libcppa::PingOrPong p;
      p.ParseFromArray(msg.buf.data(), static_cast<int>(msg.buf.size()));
      if (p.has_ping()) {
        self->send(buddy, ping_atom::value, p.ping().id());
      }
      else if (p.has_pong()) {
        self->send(buddy, pong_atom::value, p.pong().id());
      }
      else {
        self->quit(exit_reason::user_shutdown);
        cerr << "neither Ping nor Pong!" << endl;
      }
      // receive next length prefix
      self->configure_read(hdl, receive_policy::exactly(sizeof(uint32_t)));
      self->unbecome();
    }
  }.or_else(default_bhvr);
  auto await_length_prefix = message_handler {
    [=](const new_data_msg& msg) {
      uint32_t num_bytes;
      memcpy(&num_bytes, msg.buf.data(), sizeof(uint32_t));
      num_bytes = htonl(num_bytes);
      if (num_bytes > (1024 * 1024)) {
        aout(self) << "someone is trying something nasty" << endl;
        self->quit(exit_reason::user_shutdown);
        return;
      }
      // receive protobuf data
      auto nb = static_cast<size_t>(num_bytes);
      self->configure_read(hdl, receive_policy::exactly(nb));
      self->become(keep_behavior, await_protobuf_data);
    }
  }.or_else(default_bhvr);
  // initial setup
  self->configure_read(hdl, receive_policy::exactly(sizeof(uint32_t)));
  self->become(await_length_prefix);
}

behavior server(broker* self, const actor& buddy) {
  print_on_exit(self, "server");
  aout(self) << "server is running" << endl;
  return {
    [=](const new_connection_msg& msg) {
      aout(self) << "server accepted new connection" << endl;
      auto io_actor = self->fork(protobuf_io, msg.handle, buddy);
      // only accept 1 connection in our example
      self->quit();
    }
  };
}

class config : public actor_system_config {
public:
  uint16_t port = 0;
  std::string host = "localhost";
  bool server_mode = false;

  config() {
    opt_group{custom_options_, "global"}
    .add(port, "port,p", "set port")
    .add(host, "host,H", "set host (ignored in server mode)")
    .add(server_mode, "server-mode,s", "enable server mode");
  }
};

void run_server(actor_system& system, const config& cfg) {
  cout << "run in server mode" << endl;
  auto pong_actor = system.spawn(pong);
  auto server_actor = system.middleman().spawn_server(server, cfg.port,
                                                      pong_actor);
  if (!server_actor)
    cerr << "unable to spawn server: "
         << system.render(server_actor.error()) << endl;
}

void run_client(actor_system& system, const config& cfg) {
  cout << "run in client mode" << endl;
  auto ping_actor = system.spawn(ping, 20u);
  auto io_actor = system.middleman().spawn_client(protobuf_io, cfg.host,
                                                  cfg.port, ping_actor);
  if (!io_actor) {
    cout << "cannot connect to " << cfg.host << " at port " << cfg.port
         << ": " << system.render(io_actor.error()) << endl;
    return;
  }
  send_as(*io_actor, ping_actor, kickoff_atom::value, *io_actor);
}

void caf_main(actor_system& system, const config& cfg) {
  auto f = cfg.server_mode ? run_server : run_client;
  f(system, cfg);
}

} // namespace <anonymous>

CAF_MAIN(io::middleman)
