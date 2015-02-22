/******************************************************************************\
 * This example program showcases how to manually manage socket IO using      *
 * a broker. Server and client exchange integers in a 'ping-pong protocol'.   *
 *                                                                            *
 * Minimal setup:                                                             *
 * - ./build/bin/broker -s 4242                                               *
 * - ./build/bin/broker -c localhost 4242                                     *
\ ******************************************************************************/

#ifdef WIN32
# define _WIN32_WINNT 0x0600
# include <Winsock2.h>
#else
# include <arpa/inet.h> // htonl
#endif

#include <vector>
#include <string>
#include <limits>
#include <memory>
#include <cstdint>
#include <cassert>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

using ping_atom = atom_constant<atom("ping")>;
using pong_atom = atom_constant<atom("pong")>;
using kickoff_atom = atom_constant<atom("kickoff")>;

// utility function to print an exit message with custom name
void print_on_exit(const actor& ptr, const std::string& name) {
  ptr->attach_functor([=](std::uint32_t reason) {
    aout(ptr) << name << " exited with reason " << reason << endl;
  });
}

behavior ping(event_based_actor* self, size_t num_pings) {
  auto count = make_shared<size_t>(0);
  return {
    [=](kickoff_atom, const actor& pong) {
      self->send(pong, ping_atom::value, int32_t(1));
      self->become (
        [=](pong_atom, int32_t value) -> message {
          if (++*count >= num_pings) self->quit();
          return make_message(ping_atom::value, value + 1);
        }
      );
    }
  };
}

behavior pong() {
  return {
    [](ping_atom, int32_t value) {
      return make_message(pong_atom::value, value);
    }
  };
}

// utility function for sending an integer type
template <class T>
void write_int(broker* self, connection_handle hdl, T value) {
  auto cpy = static_cast<T>(htonl(value));
  self->write(hdl, sizeof(T), &cpy);
  self->flush(hdl);
}

void write_int(broker* self, connection_handle hdl, uint64_t value) {
  // write two uint32 values instead (htonl does not work for 64bit integers)
  write_int(self, hdl, static_cast<uint32_t>(value));
  write_int(self, hdl, static_cast<uint32_t>(value >> sizeof(uint32_t)));
}

// utility function for reading an ingeger from incoming data
template <class T>
void read_int(const void* data, T& storage) {
  memcpy(&storage, data, sizeof(T));
  storage = static_cast<T>(ntohl(storage));
}

void read_int(const void* data, uint64_t& storage) {
  uint32_t first;
  uint32_t second;
  read_int(data, first);
  read_int(reinterpret_cast<const char*>(data) + sizeof(uint32_t), second);
  storage = first | (static_cast<uint64_t>(second) << sizeof(uint32_t));
}

// implemenation of our broker
behavior broker_impl(broker* self, connection_handle hdl, const actor& buddy) {
  // we assume io_fsm manages a broker with exactly one connection,
  // i.e., the connection ponted to by `hdl`
  assert(self->num_connections() == 1);
  // monitor buddy to quit broker if buddy is done
  self->monitor(buddy);
  // setup: we are exchanging only messages consisting of an atom
  // (as uint64_t) and an integer value (int32_t)
  self->configure_read(hdl, receive_policy::exactly(sizeof(uint64_t) +
                            sizeof(int32_t)));
  // our message handlers
  return {
    [=](const connection_closed_msg& msg) {
      // brokers can multiplex any number of connections, however
      // this example assumes io_fsm to manage a broker with
      // exactly one connection
      if (msg.handle == hdl) {
        aout(self) << "connection closed" << endl;
        // force buddy to quit
        self->send_exit(buddy, exit_reason::remote_link_unreachable);
        self->quit(exit_reason::remote_link_unreachable);
      }
    },
    [=](atom_value av, int32_t i) {
      assert(av == ping_atom::value || av == pong_atom::value);
      aout(self) << "send {" << to_string(av) << ", " << i << "}" << endl;
      // cast atom to its underlying type, i.e., uint64_t
      write_int(self, hdl, static_cast<uint64_t>(av));
      write_int(self, hdl, i);
    },
    [=](const down_msg& dm) {
      if (dm.source == buddy) {
        aout(self) << "our buddy is down" << endl;
        // quit for same reason
        self->quit(dm.reason);
      }
    },
    [=](const new_data_msg& msg) {
      // read the atom value as uint64_t from buffer
      uint64_t atm_val;
      read_int(msg.buf.data(), atm_val);
      // cast to original type
      auto atm = static_cast<atom_value>(atm_val);
      // read integer value from buffer, jumping to the correct
      // position via offset_data(...)
      int32_t ival;
      read_int(msg.buf.data() + sizeof(uint64_t), ival);
      // show some output
      aout(self) << "received {" << to_string(atm) << ", " << ival << "}"
             << endl;
      // send composed message to our buddy
      self->send(buddy, atm, ival);
    },
    others >> [=] {
      cout << "unexpected: " << to_string(self->current_message()) << endl;
    }
  };
}

behavior server(broker* self, const actor& buddy) {
  aout(self) << "server is running" << endl;
  return {
    [=](const new_connection_msg& msg) {
      aout(self) << "server accepted new connection" << endl;
      // by forking into a new broker, we are no longer
      // responsible for the connection
      auto impl = self->fork(broker_impl, msg.handle, buddy);
      print_on_exit(impl, "broker_impl");
      aout(self) << "quit server (only accept 1 connection)" << endl;
      self->quit();
    },
    others >> [=] {
      cout << "unexpected: " << to_string(self->current_message()) << endl;
    }
  };
}

optional<uint16_t> as_u16(const std::string& str) {
  return static_cast<uint16_t>(stoul(str));
}

int main(int argc, char** argv) {
  message_builder{argv + 1, argv + argc}.apply({
    on("-s", as_u16) >> [&](uint16_t port) {
      cout << "run in server mode" << endl;
      auto pong_actor = spawn(pong);
      auto server_actor = spawn_io_server(server, port, pong_actor);
      print_on_exit(server_actor, "server");
      print_on_exit(pong_actor, "pong");
    },
    on("-c", val<string>, as_u16) >> [&](const string& host, uint16_t port) {
      auto ping_actor = spawn(ping, 20);
      auto io_actor = spawn_io_client(broker_impl, host, port, ping_actor);
      print_on_exit(io_actor, "protobuf_io");
      print_on_exit(ping_actor, "ping");
      send_as(io_actor, ping_actor, kickoff_atom::value, io_actor);
    },
    others >> [] {
      cerr << "use with eihter '-s PORT' as server or '-c HOST PORT' as client"
           << endl;
    }
  });
  await_all_actors_done();
  shutdown();
}
