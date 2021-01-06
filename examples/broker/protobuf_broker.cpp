#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#ifdef CAF_WINDOWS
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#endif

CAF_PUSH_WARNINGS
#include "pingpong.pb.h"
CAF_POP_WARNINGS

CAF_BEGIN_TYPE_ID_BLOCK(protobuf_example, first_custom_type_id)

  CAF_ADD_ATOM(protobuf_example, kickoff_atom)

CAF_END_TYPE_ID_BLOCK(protobuf_example)

namespace {

using namespace caf;
using namespace caf::io;

// utility function to print an exit message with custom name
void print_on_exit(scheduled_actor* self, const std::string& name) {
  self->attach_functor([=](const error& reason) {
    aout(self) << name << " exited: " << to_string(reason) << std::endl;
  });
}

struct ping_state {
  size_t count = 0;
};

behavior ping(stateful_actor<ping_state>* self, size_t num_pings) {
  print_on_exit(self, "ping");
  return {
    [=](kickoff_atom, const actor& pong) {
      self->send(pong, ping_atom_v, 1);
      self->become([=](pong_atom, int value) -> result<ping_atom, int> {
        if (++(self->state.count) >= num_pings)
          self->quit();
        return {ping_atom_v, value + 1};
      });
    },
  };
}

behavior pong(event_based_actor* self) {
  print_on_exit(self, "pong");
  return {
    [=](ping_atom, int value) { return make_message(pong_atom_v, value); },
  };
}

void protobuf_io(broker* self, connection_handle hdl, const actor& buddy) {
  print_on_exit(self, "protobuf_io");
  aout(self) << "protobuf broker started" << std::endl;
  self->monitor(buddy);
  self->set_down_handler([=](const down_msg& dm) {
    if (dm.source == buddy) {
      aout(self) << "our buddy is down" << std::endl;
      self->quit(dm.reason);
    }
  });
  auto write = [=](const org::caf::PingOrPong& p) {
    std::string buf = p.SerializeAsString();
    auto s = htonl(static_cast<uint32_t>(buf.size()));
    self->write(hdl, sizeof(uint32_t), &s);
    self->write(hdl, buf.size(), buf.data());
    self->flush(hdl);
  };
  auto default_callbacks = message_handler{
    [=](const connection_closed_msg&) {
      aout(self) << "connection closed" << std::endl;
      self->send_exit(buddy, exit_reason::remote_link_unreachable);
      self->quit(exit_reason::remote_link_unreachable);
    },
    [=](ping_atom, int i) {
      aout(self) << "'ping' " << i << std::endl;
      org::caf::PingOrPong p;
      p.mutable_ping()->set_id(i);
      write(p);
    },
    [=](pong_atom, int i) {
      aout(self) << "'pong' " << i << std::endl;
      org::caf::PingOrPong p;
      p.mutable_pong()->set_id(i);
      write(p);
    },
  };
  auto await_protobuf_data = message_handler {
    [=](const new_data_msg& msg) {
      org::caf::PingOrPong p;
      p.ParseFromArray(msg.buf.data(), static_cast<int>(msg.buf.size()));
      if (p.has_ping()) {
        self->send(buddy, ping_atom_v, p.ping().id());
      }
      else if (p.has_pong()) {
        self->send(buddy, pong_atom_v, p.pong().id());
      }
      else {
        self->quit(exit_reason::user_shutdown);
        std::cerr << "neither Ping nor Pong!" << std::endl;
      }
      // receive next length prefix
      self->configure_read(hdl, receive_policy::exactly(sizeof(uint32_t)));
      self->unbecome();
    },
  }.or_else(default_callbacks);
  auto await_length_prefix = message_handler {
    [=](const new_data_msg& msg) {
      uint32_t num_bytes;
      memcpy(&num_bytes, msg.buf.data(), sizeof(uint32_t));
      num_bytes = htonl(num_bytes);
      if (num_bytes > (1024 * 1024)) {
        aout(self) << "someone is trying something nasty" << std::endl;
        self->quit(exit_reason::user_shutdown);
        return;
      }
      // receive protobuf data
      auto nb = static_cast<size_t>(num_bytes);
      self->configure_read(hdl, receive_policy::exactly(nb));
      self->become(keep_behavior, await_protobuf_data);
    },
  }.or_else(default_callbacks);
  // initial setup
  self->configure_read(hdl, receive_policy::exactly(sizeof(uint32_t)));
  self->become(await_length_prefix);
}

behavior server(broker* self, const actor& buddy) {
  print_on_exit(self, "server");
  aout(self) << "server is running" << std::endl;
  return {
    [=](const new_connection_msg& msg) {
      aout(self) << "server accepted new connection" << std::endl;
      auto io_actor = self->fork(protobuf_io, msg.handle, buddy);
      // only accept 1 connection in our example
      self->quit();
    },
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
  std::cout << "run in server mode" << std::endl;
  auto pong_actor = system.spawn(pong);
  auto server_actor = system.middleman().spawn_server(server, cfg.port,
                                                      pong_actor);
  if (!server_actor)
    std::cerr << "unable to spawn server: " << to_string(server_actor.error())
              << std::endl;
}

void run_client(actor_system& system, const config& cfg) {
  std::cout << "run in client mode" << std::endl;
  auto ping_actor = system.spawn(ping, 20u);
  auto io_actor = system.middleman().spawn_client(protobuf_io, cfg.host,
                                                  cfg.port, ping_actor);
  if (!io_actor) {
    std::cout << "cannot connect to " << cfg.host << " at port " << cfg.port
              << ": " << to_string(io_actor.error()) << std::endl;
    return;
  }
  send_as(*io_actor, ping_actor, kickoff_atom_v, *io_actor);
}

void caf_main(actor_system& system, const config& cfg) {
  auto f = cfg.server_mode ? run_server : run_client;
  f(system, cfg);
}

} // namespace

CAF_MAIN(id_block::protobuf_example, io::middleman)
