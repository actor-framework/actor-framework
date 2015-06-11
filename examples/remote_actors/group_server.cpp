/******************************************************************************\
 * This example program represents a minimal IRC-like group                   *
 * communication server.                                                      *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - ./build/bin/group_server -p 4242                                         *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n alice        *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n bob          *
\ ******************************************************************************/

#include <string>
#include <cstdlib>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using namespace std;
using namespace caf;

int main(int argc, char** argv) {
  uint16_t port = 0;
  auto res = message_builder(argv + 1, argv + argc).extract_opts({
    {"port,p", "set port", port}
  });
  if (! res.error.empty()) {
    cerr << res.error << endl;
    return 1;
  }
  if (res.opts.count("help") > 0) {
    cout << res.helptext << endl;
    return 0;
  }
  if (! res.remainder.empty()) {
    // not all CLI arguments could be consumed
    cerr << "*** too many arguments" << endl << res.helptext << endl;
    return 1;
  }
  if (res.opts.count("port") == 0 || port <= 1024) {
    cerr << "*** no valid port (>1024) given" << endl << res.helptext << endl;
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
