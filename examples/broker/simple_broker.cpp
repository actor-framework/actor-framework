/******************************************************************************\
 * This example program showcases how to manually manage socket IO using      *
 * a broker. Server and client exchange integers in a 'ping-pong protocol'.   *
 *                                                                            *
 * Minimal setup:                                                             *
 * - ./build/example/simple_broker -s 4242                                    *
 * - ./build/example/simple_broker -c localhost 4242                          *
\******************************************************************************/

// Manual refs: 42-47 (Actors.tex)

#include "caf/config.hpp"

#ifdef CAF_WINDOWS
#  define _WIN32_WINNT 0x0600
#  include <winsock2.h>
#else
#  include <arpa/inet.h> // htonl
#endif

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cerr;
using std::cout;
using std::endl;

using namespace caf;
using namespace caf::io;

namespace {

// Utility function to print an exit message with custom name.
void print_on_exit(const actor& hdl, const std::string& name) {
  hdl->attach_functor([=](const error& reason) {
    cout << name << " exited: " << to_string(reason) << endl;
  });
}

enum class op : uint8_t {
  ping,
  pong,
};

behavior ping(event_based_actor* self, size_t num_pings) {
  auto count = std::make_shared<size_t>(0);
  return {
    [=](ok_atom, const actor& pong) {
      self->send(pong, ping_atom_v, int32_t(1));
      self->become([=](pong_atom, int32_t value) -> result<ping_atom, int32_t> {
        if (++*count >= num_pings)
          self->quit();
        return {ping_atom_v, value + 1};
      });
    },
  };
}

behavior pong() {
  return {
    [](ping_atom, int32_t value) -> result<pong_atom, int32_t> {
      return {pong_atom_v, value};
    },
  };
}

// Utility function for sending an integer type.
template <class T>
void write_int(broker* self, connection_handle hdl, T value) {
  using unsigned_type = typename std::make_unsigned<T>::type;
  auto cpy = static_cast<T>(htonl(static_cast<unsigned_type>(value)));
  self->write(hdl, sizeof(T), &cpy);
  self->flush(hdl);
}

// Utility function for reading an ingeger from incoming data.
template <class T>
void read_int(const void* data, T& storage) {
  using unsigned_type = typename std::make_unsigned<T>::type;
  memcpy(&storage, data, sizeof(T));
  storage = static_cast<T>(ntohl(static_cast<unsigned_type>(storage)));
}

// Implementation of our broker.
behavior broker_impl(broker* self, connection_handle hdl, const actor& buddy) {
  // We assume io_fsm manages a broker with exactly one connection,
  // i.e., the connection ponted to by `hdl`.
  assert(self->num_connections() == 1);
  // Monitor buddy to quit broker if buddy is done.
  self->monitor(buddy);
  self->set_down_handler([=](down_msg& dm) {
    if (dm.source == buddy) {
      aout(self) << "our buddy is down" << endl;
      // Quit for same reason.
      self->quit(dm.reason);
    }
  });
  // Setup: we are exchanging only messages consisting of an atom
  // (as uint64_t) and an integer value (int32_t).
  self->configure_read(
    hdl, receive_policy::exactly(sizeof(uint64_t) + sizeof(int32_t)));
  // Our message handlers.
  return {
    [=](const connection_closed_msg& msg) {
      // Brokers can multiplex any number of connections, however
      // this example assumes io_fsm to manage a broker with
      // exactly one connection.
      if (msg.handle == hdl) {
        aout(self) << "connection closed" << endl;
        // force buddy to quit
        self->send_exit(buddy, exit_reason::remote_link_unreachable);
        self->quit(exit_reason::remote_link_unreachable);
      }
    },
    [=](ping_atom, int32_t i) {
      aout(self) << "send {ping, " << i << "}" << endl;
      write_int(self, hdl, static_cast<uint8_t>(op::ping));
      write_int(self, hdl, i);
    },
    [=](pong_atom, int32_t i) {
      aout(self) << "send {pong, " << i << "}" << endl;
      write_int(self, hdl, static_cast<uint8_t>(op::pong));
      write_int(self, hdl, i);
    },
    [=](const new_data_msg& msg) {
      // Read the operation value as uint8_t from buffer.
      uint8_t op_val;
      read_int(msg.buf.data(), op_val);
      // Read integer value from buffer, jumping to the correct
      // position via offset_data(...).
      int32_t ival;
      read_int(msg.buf.data() + sizeof(uint8_t), ival);
      // Show some output.
      aout(self) << "received {" << op_val << ", " << ival << "}" << endl;
      // Send composed message to our buddy.
      switch (static_cast<op>(op_val)) {
        case op::ping:
          self->send(buddy, ping_atom_v, ival);
          break;
        case op::pong:
          self->send(buddy, pong_atom_v, ival);
          break;
        default:
          aout(self) << "invalid value for op_val, stop" << endl;
          self->quit(sec::invalid_argument);
      }
    },
  };
}

behavior server(broker* self, const actor& buddy) {
  aout(self) << "server is running" << endl;
  return {
    [=](const new_connection_msg& msg) {
      aout(self) << "server accepted new connection" << endl;
      // By forking into a new broker, we are no longer
      // responsible for the connection.
      auto impl = self->fork(broker_impl, msg.handle, buddy);
      print_on_exit(impl, "broker_impl");
      aout(self) << "quit server (only accept 1 connection)" << endl;
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
  cout << "run in server mode" << endl;
  auto pong_actor = system.spawn(pong);
  auto server_actor = system.middleman().spawn_server(server, cfg.port,
                                                      pong_actor);
  if (!server_actor) {
    std::cerr << "failed to spawn server: " << to_string(server_actor.error())
              << endl;
    return;
  }
  print_on_exit(*server_actor, "server");
  print_on_exit(pong_actor, "pong");
}

void run_client(actor_system& system, const config& cfg) {
  auto ping_actor = system.spawn(ping, size_t{20});
  auto io_actor = system.middleman().spawn_client(broker_impl, cfg.host,
                                                  cfg.port, ping_actor);
  if (!io_actor) {
    std::cerr << "failed to spawn client: " << to_string(io_actor.error())
              << endl;
    return;
  }
  print_on_exit(ping_actor, "ping");
  print_on_exit(*io_actor, "protobuf_io");
  send_as(*io_actor, ping_actor, ok_atom_v, *io_actor);
}

void caf_main(actor_system& system, const config& cfg) {
  auto f = cfg.server_mode ? run_server : run_client;
  f(system, cfg);
}

} // namespace

CAF_MAIN(io::middleman)
