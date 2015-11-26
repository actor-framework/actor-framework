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
  if (! res.error.empty())
    return cerr << res.error << endl, 1;
  if (res.opts.count("help") > 0)
    return cout << res.helptext << endl, 0;
  if (! res.remainder.empty())
    return cerr << "*** too many arguments" << endl << res.helptext << endl, 1;
  if (res.opts.count("port") == 0 || port <= 1024)
    return cerr << "*** no valid port given" << endl << res.helptext << endl, 1;
  actor_system system{actor_system_config{}.load<io::middleman>()};
  auto pres = system.middleman().publish_local_groups(port);
  if (! pres)
    return cerr << "*** error: " << pres.error().message() << endl, 1;
  cout << "type 'quit' to shutdown the server" << endl;
  string line;
  while (getline(cin, line))
    if (line == "quit")
      return 0;
    else
      cerr << "illegal command" << endl;
}
