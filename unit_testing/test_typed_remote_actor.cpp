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
  catch (network_error& e) {
    cout << e.what() << endl;
    CAF_CHECKPOINT();
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
  auto port = io::typed_publish(spawn_typed(server), 0, "127.0.0.1");
  CAF_PRINT("running on port " << port);
  return port;
}

int main(int argc, char** argv) {
  CAF_TEST(test_typed_remote_actor);
  announce<ping>("ping", &ping::value);
  announce<pong>("pong", &pong::value);
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
      // execute client_part() in a separate process,
      // connected via localhost socket
      scoped_actor self;
      auto child = run_program(self, argv[0], "-c", port);
      CAF_CHECKPOINT();
      child.join();
      self->await_all_other_actors_done();
      self->receive(
        [](const std::string& output) {
          cout << endl << endl << "*** output of client program ***"
               << endl << output << endl;
        }
      );
    }
  });
  shutdown();
  return CAF_TEST_RESULT();
}
