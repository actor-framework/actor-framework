#include <thread>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;

struct ping {
  int32_t value;
};

bool operator==(const ping& lhs, const ping& rhs) {
  return lhs.value == rhs.value;
}

struct pong {
  int32_t value;

};

bool operator==(const pong& lhs, const pong& rhs) {
  return lhs.value == rhs.value;
}

using server_type = typed_actor<replies_to<ping>::with<pong>>;

using client_type = typed_actor<>;

server_type::behavior_type server() {
  return {
    [](const ping & p)->pong{CAF_CHECKPOINT();
      return pong{p.value};
    }
  };
}

void run_client(const char* host, uint16_t port) {
  // check whether invalid_argument is thrown
  // when trying to connect to get an untyped
  // handle to the server
  try {
    io::remote_actor(host, port);
  }
  catch (std::runtime_error& e) {
    cout << e.what() << endl;
    CAF_CHECKPOINT();
  }
  catch (std::exception& e) {
    cerr << "unexpected: " << e.what() << endl;
  }
  CAF_CHECKPOINT();
  auto serv = io::typed_remote_actor<server_type>(host, port);
  CAF_CHECKPOINT();
  scoped_actor self;
  self->sync_send(serv, ping{42})
    .await([](const pong& p) { CAF_CHECK_EQUAL(p.value, 42); });
  anon_send_exit(serv, exit_reason::user_shutdown);
  self->monitor(serv);
  self->receive([&](const down_msg& dm) {
    CAF_CHECK_EQUAL(dm.reason, exit_reason::user_shutdown);
    CAF_CHECK(dm.source == serv);
  });
}

uint16_t run_server() {
  auto ref = spawn_typed(server);
  uint16_t port = 4242;
  for (;;) {
    try {
      io::typed_publish(ref, port, "127.0.0.1");
      CAF_PRINT("running on port " << port);
      return port;
    }
    catch (bind_failure&) {
      // try next port
      ++port;
    }
  }
}

int main(int argc, char** argv) {
  announce<ping>(&ping::value);
  announce<pong>(&pong::value);
  message_builder{argv + 1, argv + argc}.apply({
    on("-c", spro<uint16_t>)>> [](uint16_t port) {
      CAF_PRINT("run in client mode");
      run_client("localhost", port);
    },
    on("-s") >> [] {
      run_server();
    },
    on() >> [&] {
      auto port = run_server();
      CAF_CHECKPOINT();
      ostringstream oss;
      oss << argv[0] << " -c " << port << to_dev_null;
      // execute client_part() in a separate process,
      // connected via localhost socket
      auto child = thread([&oss]() {
        CAF_LOGC_TRACE("NONE", "main$thread_launcher", "");
        string cmdstr = oss.str();
        if (system(cmdstr.c_str()) != 0) {
          CAF_PRINTERR("FATAL: command \"" << cmdstr << "\" failed!");
          abort();
        }
      });
      CAF_CHECKPOINT();
      child.join();
    }
  });
  CAF_CHECKPOINT();
  await_all_actors_done();
  shutdown();
  return CAF_TEST_RESULT();
}
