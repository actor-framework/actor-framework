/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <memory>
#include <iostream>

#include "test.hpp"
#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;
using namespace caf::io;

void ping(event_based_actor* self, size_t num_pings) {
  CAF_PRINT("num_pings: " << num_pings);
  auto count = std::make_shared<size_t>(0);
  self->become(
    on(atom("kickoff"), arg_match) >> [=](const actor& pong) {
      CAF_CHECKPOINT();
      self->send(pong, atom("ping"), 1);
      self->become(
      on(atom("pong"), arg_match) >>
      [=](int value)->std::tuple<atom_value, int> {
        if (++*count >= num_pings) {
          CAF_PRINT("received " << num_pings
                 << " pings, call self->quit");
          self->quit();
        }
        return std::make_tuple(atom("ping"), value + 1);
      },
      others() >> CAF_UNEXPECTED_MSG_CB(self));
    },
    others() >> CAF_UNEXPECTED_MSG_CB(self)
  );
}

void pong(event_based_actor* self) {
  CAF_CHECKPOINT();
  self->become(
    on(atom("ping"), arg_match) >> [=](int value)
                       ->std::tuple<atom_value, int> {
      CAF_CHECKPOINT();
      self->monitor(self->last_sender());
      // set next behavior
      self->become(
        on(atom("ping"), arg_match) >> [](int val) {
          return std::make_tuple(atom("pong"), val);
        },
        [=](const down_msg& dm) {
          CAF_PRINT("received down_msg{" << dm.reason << "}");
          self->quit(dm.reason);
        },
        others() >> CAF_UNEXPECTED_MSG_CB(self)
      );
      // reply to 'ping'
      return std::make_tuple(atom("pong"), value);
    },
    others() >> CAF_UNEXPECTED_MSG_CB(self));
}

void peer_fun(broker* self, connection_handle hdl, const actor& buddy) {
  CAF_CHECKPOINT();
  CAF_CHECK(self != nullptr);
  CAF_CHECK(buddy != invalid_actor);
  self->monitor(buddy);
  // assume exactly one connection
  auto cons = self->connections();
  if (cons.size() != 1) {
    cerr << "expected 1 connection, found " << cons.size() << endl;
    throw std::logic_error("num_connections() != 1");
  }
  self->configure_read(
    hdl, receive_policy::exactly(sizeof(atom_value) + sizeof(int)));
  auto write = [=](atom_value type, int value) {
    CAF_LOGF_DEBUG("write: " << value);
    auto& buf = self->wr_buf(hdl);
    auto first = reinterpret_cast<char*>(&type);
    buf.insert(buf.end(), first, first + sizeof(atom_value));
    first = reinterpret_cast<char*>(&value);
    buf.insert(buf.end(), first, first + sizeof(int));
    self->flush(hdl);

  };
  self->become(
    [=](const connection_closed_msg&) {
      CAF_PRINT("received connection_closed_msg");
      self->quit();
    },
    [=](const new_data_msg& msg) {
      CAF_PRINT("received new_data_msg");
      atom_value type;
      int value;
      memcpy(&type, msg.buf.data(), sizeof(atom_value));
      memcpy(&value, msg.buf.data() + sizeof(atom_value), sizeof(int));
      self->send(buddy, type, value);
    },
    on(atom("ping"), arg_match) >> [=](int value) {
      CAF_PRINT("received ping{" << value << "}");
      write(atom("ping"), value);
    },
    on(atom("pong"), arg_match) >> [=](int value) {
      CAF_PRINT("received pong{" << value << "}");
      write(atom("pong"), value);
    },
    [=](const down_msg& dm) {
      CAF_PRINT("received down_msg");
      if (dm.source == buddy) self->quit(dm.reason);
    },
    others() >> CAF_UNEXPECTED_MSG_CB(self)
  );
}

behavior peer_acceptor_fun(broker* self, const actor& buddy) {
  CAF_CHECKPOINT();
  return {
    [=](const new_connection_msg& msg) {
      CAF_CHECKPOINT();
      CAF_PRINT("received new_connection_msg");
      self->fork(peer_fun, msg.handle, buddy);
      self->quit();
    },
    others() >> CAF_UNEXPECTED_MSG_CB(self)
  };
}

void run_server(bool spawn_client, const char* bin_path) {
  auto p = spawn(pong);
  uint16_t port = 4242;
  bool done = false;
  while (!done) {
    try {
      io::spawn_io_server(peer_acceptor_fun, port, p);
    }
    catch (bind_failure&) {
      // try next port
      ++port;
    }
    done = true;
  }
  CAF_CHECKPOINT();
  cout << "server is running on port " << port << endl;
  if (spawn_client) {
    ostringstream oss;
    oss << bin_path << " -c " << port << to_dev_null;
    thread child{[&oss] {
      CAF_LOGC_TRACE("NONE", "main$thread_launcher", "");
      auto cmdstr = oss.str();
      if (system(cmdstr.c_str()) != 0) {
        CAF_PRINTERR("FATAL: command failed: " << cmdstr);
        abort();
      }
    }};
    CAF_CHECKPOINT();
    child.join();
  }
}

int main(int argc, char** argv) {
  CAF_TEST(test_broker);
  message_builder{argv + 1, argv + argc}.apply({
     on("-c", arg_match) >> [&](const std::string& portstr) {
      auto port = static_cast<uint16_t>(std::stoi(portstr));
      auto p = spawn(ping, 10);
      CAF_CHECKPOINT();
      auto cl = spawn_io_client(peer_fun, "localhost", port, p);
      CAF_CHECKPOINT();
      anon_send(p, atom("kickoff"), cl);
      CAF_CHECKPOINT();
    },
    on("-s")  >> [&] {
      run_server(false, argv[0]);
    },
    on() >> [&] {
      run_server(true, argv[0]);

    },
    others() >> [&] {
       cerr << "usage: " << argv[0] << " [-c PORT]" << endl;
    }
  });
  CAF_CHECKPOINT();
  await_all_actors_done();
  CAF_CHECKPOINT();
  shutdown();
  return CAF_TEST_RESULT();
}
