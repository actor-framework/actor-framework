/******************************************************************************\
 * This example program represents a minimal IRC-like group                   *
 * communication server.                                                      *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - ./build/bin/group_server -p 4242                                         *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n alice        *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n bob          *
\ ******************************************************************************/

#include <cstdlib>
#include <string>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;

optional<uint16_t> to_port(const string& arg) {
  char* last = nullptr;
  auto res = strtol(arg.c_str(), &last, 10);
  if (last == (arg.c_str() + arg.size()) && res > 1024) {
    return res;
  }
  return none;
}

optional<uint16_t> long_port(const string& arg) {
  if (arg.compare(0, 7, "--port=") == 0) {
    return to_port(arg.substr(7));
  }
  return none;
}

int main(int argc, char** argv) {
  uint16_t port = 0;
  message_builder{argv + 1, argv + argc}.apply({
    (on("-p", to_port) || on(long_port)) >> [&](uint16_t arg) {
      port = arg;
    }
  });
  if (port <= 1024) {
    cerr << "*** no port > 1024 given" << endl;
    cout << "options:" << endl
         << "  -p <arg1> | --port=<arg1>               set port" << endl;
    return 1;
  }
  try {
    // try to bind the group server to the given port,
    // this allows other nodes to access groups of this server via
    // group::get("remote", "<group>@<host>:<port>");
    // note: it is not needed to explicitly create a <group> on the server,
    //     as groups are created on-the-fly on first usage
    io::publish_local_groups(port);
  }
  catch (bind_failure& e) {
    // thrown if <port> is already in use
    cerr << "*** bind_failure: " << e.what() << endl;
    return 2;
  }
  catch (network_error& e) {
    // thrown on errors in the socket API
    cerr << "*** network error: " << e.what() << endl;
    return 2;
  }
  cout << "type 'quit' to shutdown the server" << endl;
  string line;
  while (getline(cin, line)) {
    if (line == "quit") {
      return 0;
    }
    else {
      cerr << "illegal command" << endl;
    }
  }
  shutdown();
}
