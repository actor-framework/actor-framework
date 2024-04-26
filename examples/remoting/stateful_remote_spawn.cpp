// This program illustrates how to spawn a simple aggregator
// across the network.
//
// Run server at port 4242:
// - remote_spawn -s -p 4242
//
// Run client at the same host:
// - remote_spawn -H localhost -p 4242

#include "caf/io/middleman.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/scoped_actor.hpp"

#include <iostream>
#include <string>
#include <vector>

struct aggregator_trait {
  using signatures = caf::type_list<caf::result<void>(caf::add_atom, int32_t),
                                    caf::result<int32_t>(caf::get_atom)>;
};

using aggregator = caf::typed_actor<aggregator_trait>;

CAF_BEGIN_TYPE_ID_BLOCK(remote_spawn, first_custom_type_id)

  CAF_ADD_TYPE_ID(remote_spawn, (aggregator))

CAF_END_TYPE_ID_BLOCK(remote_spawn)

using std::string;

using namespace caf;
using namespace std::literals;

struct aggregator_state {
  aggregator_state() = default;
  explicit aggregator_state(int32_t init) : value(init) {
    // nop
  }

  aggregator::behavior_type make_behavior() {
    return {
      [this](add_atom, int32_t a) { value += a; },
      [this](get_atom) -> result<int32_t> { return value; },
    };
  }

  int32_t value = 0;
};

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
void client_repl(actor_system& sys, aggregator hdl) {
  auto usage = [&sys] {
    sys.println("Usage:");
    sys.println("  quit     : terminate program");
    sys.println("  add <x>  : adds x to remote aggregator");
    sys.println("  get      : prints the aggregated value");
    sys.println("");
  };
  usage();
  scoped_actor self{sys};
  self->link_to(hdl);
  // read next line, split it, and evaluate user input
  string line;
  while (std::getline(std::cin, line)) {
    line = trim(std::move(line));
    if (line == "quit")
      return;
    std::vector<string> words;
    split(words, line, is_any_of(" "), token_compress_on);
    if (words.size() > 2) {
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
    if (words[0] == "add" && words.size() == 2) {
      auto x = to_int32_t(words[1]);
      if (!x) {
        usage();
        continue;
      }
      self->mail(add_atom_v, *x).send(hdl);
    } else if (words[0] == "get") {
      if (auto value = self->mail(get_atom_v).request(hdl, 1s).receive()) {
        sys.println("Aggregated: {}", *value);
      } else {
        sys.println("Error fetching value: {}", value.error());
      }
    } else {
      usage();
    }
  }
}

constexpr uint16_t default_port = 0;
constexpr std::string_view default_host = "localhost";
constexpr bool default_server_mode = false;

struct config : actor_system_config {
  config() {
    // Constructor parameters are listed after naming the actor.
    add_actor_type("aggregator", actor_from_state<aggregator_state>,
                   type_list_v<>, type_list_v<int32_t>);
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

void server(actor_system& sys, const config& cfg) {
  const auto port = get_or(cfg, "port", default_port);
  auto res = sys.middleman().open(port);
  if (!res) {
    sys.println("*** cannot open port: {}", to_string(res.error()));
    return;
  }
  sys.println("*** running on port: {}", *res);
  sys.println("*** press <enter> to shutdown server");
  getchar();
}

void client(actor_system& sys, const config& cfg) {
  auto host = get_or(cfg, "host", default_host);
  auto port = get_or(cfg, "port", default_port);
  auto node = sys.middleman().connect(host, port);
  if (!node) {
    sys.println("*** connect failed: {}", node.error());
    return;
  }
  auto type = "aggregator";               // type of the actor we wish to spawn
  auto args = make_message(int32_t{100}); // arguments to construct the actor
  auto tout = std::chrono::seconds(30);   // wait no longer than 30s
  auto worker = sys.middleman().remote_spawn<aggregator>(*node, type, args,
                                                         tout);
  if (!worker) {
    sys.println("*** remote spawn failed: {}", worker.error());
    return;
  }
  // start using worker in main loop
  client_repl(sys, *worker);
  // be a good citizen and terminate remotely spawned actor before exiting
  anon_send_exit(*worker, exit_reason::kill);
}

void caf_main(actor_system& sys, const config& cfg) {
  const auto server_mode = get_or(cfg, "server-mode", default_server_mode);
  auto f = server_mode ? server : client;
  f(sys, cfg);
}

CAF_MAIN(id_block::remote_spawn, io::middleman)
