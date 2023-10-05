// This program illustrates how to spawn a simple calculator
// across the network.
//
// Run server at port 4242:
// - remote_spawn -s -p 4242
//
// Run client at the same host:
// - remote_spawn -H localhost -p 4242

#include "caf/io/all.hpp"

#include "caf/all.hpp"

#include <array>
#include <cassert>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// --(rst-calculator-begin)--
using calculator
  = caf::typed_actor<caf::result<int32_t>(caf::add_atom, int32_t, int32_t),
                     caf::result<int32_t>(caf::sub_atom, int32_t, int32_t)>;
// --(rst-calculator-end)--

CAF_BEGIN_TYPE_ID_BLOCK(remote_spawn, first_custom_type_id)

  CAF_ADD_TYPE_ID(remote_spawn, (calculator))

CAF_END_TYPE_ID_BLOCK(remote_spawn)

using std::cerr;
using std::cout;
using std::endl;
using std::string;

using namespace caf;

calculator::behavior_type calculator_fun(calculator::pointer self) {
  return {
    [=](add_atom, int32_t a, int32_t b) {
      aout(self) << "received task from a remote node" << endl;
      return a + b;
    },
    [=](sub_atom, int32_t a, int32_t b) {
      aout(self) << "received task from a remote node" << endl;
      return a - b;
    },
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
         << "  <x> + <y>             : adds two int32_tegers" << endl
         << "  <x> - <y>             : subtracts two int32_tegers" << endl
         << endl;
  };
  usage();
  // read next line, split it, and evaluate user input
  string line;
  while (std::getline(std::cin, line)) {
    line = trim(std::move(line));
    if (line == "quit")
      return;
    std::vector<string> words;
    split(words, line, is_any_of(" "), token_compress_on);
    if (words.size() != 3) {
      usage();
      continue;
    }
    auto to_int32_t = [](const string& str) -> std::optional<int32_t> {
      char* end = nullptr;
      auto res = strtol(str.c_str(), &end, 10);
      if (end == str.c_str() + str.size())
        return static_cast<int32_t>(res);
      return std::nullopt;
    };
    auto x = to_int32_t(words[0]);
    auto y = to_int32_t(words[2]);
    if (!x || !y || (words[1] != "+" && words[1] != "-"))
      usage();
    else
      cout << " = "
           << caf::to_string(words[1] == "+" ? f(add_atom_v, *x, *y)
                                             : f(sub_atom_v, *x, *y))
           << std::endl;
  }
}

constexpr uint16_t default_port = 0;
constexpr std::string_view default_host = "localhost";
constexpr bool default_server_mode = false;

struct config : actor_system_config {
  config() {
    add_actor_type("calculator", calculator_fun);
    opt_group{custom_options_, "global"}
      .add<uint16_t>("port,p", "set port")
      .add<std::string>("host,H", "set node (ignored in server mode)")
      .add<bool>("server-mode,s", "enable server mode");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    put_missing(result, "port", default_port);
    put_missing(result, "host", default_host);
    put_missing(result, "server-mode", default_server_mode);
    return result;
  }
};

void server(actor_system& system, const config& cfg) {
  const auto port = get_or(cfg, "port", default_port);
  auto res = system.middleman().open(port);
  if (!res) {
    cerr << "*** cannot open port: " << to_string(res.error()) << endl;
    return;
  }
  cout << "*** running on port: " << *res << endl
       << "*** press <enter> to shutdown server" << endl;
  getchar();
}

// --(rst-client-begin)--
void client(actor_system& system, const config& cfg) {
  auto host = get_or(cfg, "host", default_host);
  auto port = get_or(cfg, "port", default_port);
  auto node = system.middleman().connect(host, port);
  if (!node) {
    cerr << "*** connect failed: " << to_string(node.error()) << endl;
    return;
  }
  auto type = "calculator";             // type of the actor we wish to spawn
  auto args = make_message();           // arguments to construct the actor
  auto tout = std::chrono::seconds(30); // wait no longer than 30s
  auto worker = system.middleman().remote_spawn<calculator>(*node, type, args,
                                                            tout);
  if (!worker) {
    cerr << "*** remote spawn failed: " << to_string(worker.error()) << endl;
    return;
  }
  // start using worker in main loop
  client_repl(make_function_view(*worker));
  // be a good citizen and terminate remotely spawned actor before exiting
  anon_send_exit(*worker, exit_reason::kill);
}
// --(rst-client-end)--

void caf_main(actor_system& system, const config& cfg) {
  const auto server_mode = get_or(cfg, "server-mode", default_server_mode);
  auto f = server_mode ? server : client;
  f(system, cfg);
}

CAF_MAIN(id_block::remote_spawn, io::middleman)
