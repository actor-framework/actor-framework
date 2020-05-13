// This example program represents a minimal IRC-like group communication
// server.
//
// Setup for a minimal chat between "alice" and "bob":
// - ./build/bin/group_server -p 4242
// - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n alice
// - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n bob

#include <string>
#include <cstdlib>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;

namespace {

class config : public actor_system_config {
public:
  uint16_t port = 0;

  config() {
    opt_group{custom_options_, "global"}
    .add(port, "port,p", "set port");
  }
};

void caf_main(actor_system& system, const config& cfg) {
  system.middleman().publish_local_groups(cfg.port);
  cout << "type 'quit' to shutdown the server" << endl;
  string line;
  while (getline(cin, line))
    if (line == "quit")
      return;
    else
      cerr << "illegal command" << endl;
}

} // namespace

CAF_MAIN(io::middleman)
