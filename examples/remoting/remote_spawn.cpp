// This program illustrates how to spawn a simple calculator
// across the network.
//
// Run server at port 4242:
// - remote_spawn -s -p 4242
//
// Run client at the same host:
// - remote_spawn -H localhost -p 4242

// Manual refs: 33-39, 99-101,106,110 (ConfiguringActorApplications)
//              125-143 (RemoteSpawn)

#include <array>
#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <iostream>
#include <functional>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

using namespace caf;

namespace {

using add_atom = atom_constant<atom("add")>;
using sub_atom = atom_constant<atom("sub")>;

using calculator = typed_actor<replies_to<add_atom, int, int>::with<int>,
                               replies_to<sub_atom, int, int>::with<int>>;

calculator::behavior_type calculator_fun(calculator::pointer self) {
  return {
    [=](add_atom, int a, int b) -> int {
      aout(self) << "received task from a remote node" << endl;
      return a + b;
    },
    [=](sub_atom, int a, int b) -> int {
      aout(self) << "received task from a remote node" << endl;
      return a - b;
    }
  };
}

// removes leading and trailing whitespaces
string trim(string s) {
  auto not_space = [](char c) { return isspace(c) == 0; };
  // trim left
  s.erase(s.begin(), find_if(s.begin(), s.end(), not_space));
  // trim right
  s.erase(find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

// implements our main loop for reading user input
void client_repl(function_view<calculator> f) {
  auto usage = [] {
  cout << "Usage:" << endl
       << "  quit                  : terminate program" << endl
       << "  <x> + <y>             : adds two integers" << endl
       << "  <x> - <y>             : subtracts two integers" << endl << endl;
  };
  usage();
  // read next line, split it, and evaluate user input
  string line;
  while (std::getline(std::cin, line)) {
    if ((line = trim(std::move(line))) == "quit")
      return;
    std::vector<string> words;
    split(words, line, is_any_of(" "), token_compress_on);
    if (words.size() != 3) {
      usage();
      continue;
    }
    auto to_int = [](const string& str) -> optional<int> {
      char* end = nullptr;
      auto res = strtol(str.c_str(), &end, 10);
      if (end == str.c_str() + str.size())
        return static_cast<int>(res);
      return none;
    };
    auto x = to_int(words[0]);
    auto y = to_int(words[2]);
    if (!x || !y || (words[1] != "+" && words[1] != "-"))
      usage();
    else
      cout << " = " << (words[1] == "+" ? f(add_atom::value, *x, *y)
                                        : f(sub_atom::value, *x, *y)) << "\n";
  }
}

struct config : actor_system_config {
  config() {
    add_actor_type("calculator", calculator_fun);
    opt_group{custom_options_, "global"}
    .add(port, "port,p", "set port")
    .add(host, "host,H", "set node (ignored in server mode)")
    .add(server_mode, "server-mode,s", "enable server mode");
  }
  uint16_t port = 0;
  string host = "localhost";
  bool server_mode = false;
};

void server(actor_system& system, const config& cfg) {
  auto res = system.middleman().open(cfg.port);
  if (!res) {
    cerr << "*** cannot open port: "
         << system.render(res.error()) << endl;
    return;
  }
  cout << "*** running on port: "
       << *res << endl
       << "*** press <enter> to shutdown server" << endl;
  getchar();
}

void client(actor_system& system, const config& cfg) {
  auto node = system.middleman().connect(cfg.host, cfg.port);
  if (!node) {
    cerr << "*** connect failed: "
         << system.render(node.error()) << endl;
    return;
  }
  auto type = "calculator"; // type of the actor we wish to spawn
  auto args = make_message(); // arguments to construct the actor
  auto tout = std::chrono::seconds(30); // wait no longer than 30s
  auto worker = system.middleman().remote_spawn<calculator>(*node, type,
                                                            args, tout);
  if (!worker) {
    cerr << "*** remote spawn failed: "
         << system.render(worker.error()) << endl;
    return;
  }
  // start using worker in main loop
  client_repl(make_function_view(*worker));
  // be a good citizen and terminate remotely spawned actor before exiting
  anon_send_exit(*worker, exit_reason::kill);
}

void caf_main(actor_system& system, const config& cfg) {
  auto f = cfg.server_mode ? server : client;
  f(system, cfg);
}

} // namespace

CAF_MAIN(io::middleman)
