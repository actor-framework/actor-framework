//  This program is a distributed version of the math_actor example.
// Client and server use a stateless request/response protocol and the client
// is failure resilient by using a FIFO request queue.
// The client auto-reconnects and also allows for server reconfiguration.
//
// Run server at port 4242:
// - ./build/bin/distributed_math_actor -s -p 4242
//
// Run client at the same host:
// - ./build/bin/distributed_math_actor -c -p 4242

// Manual refs: 250-262 (ConfiguringActorSystems)

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

using plus_atom = atom_constant<atom("plus")>;
using minus_atom = atom_constant<atom("minus")>;
using result_atom = atom_constant<atom("result")>;
using rebind_atom = atom_constant<atom("rebind")>;
using reconnect_atom = atom_constant<atom("reconnect")>;

// our "service"
behavior calculator_fun() {
  return {
    [](plus_atom, int a, int b) -> message {
      return make_message(result_atom::value, a + b);
    },
    [](minus_atom, int a, int b) -> message {
      return make_message(result_atom::value, a - b);
    }
  };
}

class client_impl : public event_based_actor {
public:
  client_impl(actor_config& cfg, string hostaddr, uint16_t port)
      : event_based_actor(cfg),
        server_(unsafe_actor_handle_init),
        host_(std::move(hostaddr)),
        port_(port) {
    set_default_handler(skip);
  }

  behavior make_behavior() override {
    become(awaiting_task());
    become(keep_behavior, reconnecting());
    return {};
  }

private:
  void request_task(atom_value op, int lhs, int rhs) {
    request(server_, infinite, op, lhs, rhs).then(
      [=](result_atom, int result) {
        aout(this) << lhs << (op == plus_atom::value ? " + " : " - ")
                   << rhs << " = " << result << endl;
      },
      [=](const error& err) {
        if (err == sec::request_receiver_down) {
          aout(this) << "*** server down, try to reconnect ..." << endl;
          // try requesting this again after successful reconnect
          become(keep_behavior,
                 reconnecting([=] { request_task(op, lhs, rhs); }));
          return;
        }
        aout(this) << "*** request resulted in error: "
                   << system().render(err) << endl;
      }

    );
  }

  behavior awaiting_task() {
    return {
      [=](atom_value op, int lhs, int rhs) {
        if (op != plus_atom::value && op != minus_atom::value) {
          return;
        }
        request_task(op, lhs, rhs);
      },
      [=](rebind_atom, string& nhost, uint16_t nport) {
        aout(this) << "*** rebind to " << nhost << ":" << nport << endl;
        using std::swap;
        swap(host_, nhost);
        swap(port_, nport);
        become(keep_behavior, reconnecting());
      }
    };
  }

  behavior reconnecting(std::function<void()> continuation = nullptr) {
    using std::chrono::seconds;
    auto mm = system().middleman().actor_handle();
    send(mm, connect_atom::value, host_, port_);
    return {
      [=](ok_atom, node_id&, strong_actor_ptr& new_server, std::set<std::string>&) {
        if (! new_server) {
          aout(this) << "*** received invalid remote actor" << endl;
          return;
        }
        aout(this) << "*** connection succeeded, awaiting tasks" << endl;
        server_ = actor_cast<actor>(new_server);
        // return to previous behavior
        if (continuation) {
          continuation();
        }
        unbecome();
      },
      [=](const error& err) {
        aout(this) << "*** could not connect to " << host_
                   << " at port " << port_
                   << ": " << system().render(err)
                   << " [try again in 3s]"
                   << endl;
        delayed_send(mm, seconds(3), connect_atom::value, host_, port_);
      },
      [=](rebind_atom, string& nhost, uint16_t nport) {
        aout(this) << "*** rebind to " << nhost << ":" << nport << endl;
        using std::swap;
        swap(host_, nhost);
        swap(port_, nport);
        auto send_mm = [=] {
          unbecome();
          send(mm, connect_atom::value, host_, port_);
        };
        // await pending ok/error message first, then send new request to MM
        become(
          keep_behavior,
          [=](ok_atom&, actor_addr&) {
            send_mm();
          },
          [=](const error&) {
            send_mm();
          }
        );
      }
    };
  }

  actor server_;
  string host_;
  uint16_t port_;
};

// removes leading and trailing whitespaces
string trim(std::string s) {
  auto not_space = [](char c) { return ! isspace(c); };
  // trim left
  s.erase(s.begin(), find_if(s.begin(), s.end(), not_space));
  // trim right
  s.erase(find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

// tries to convert `str` to an int
optional<int> toint(const string& str) {
  char* end;
  auto result = static_cast<int>(strtol(str.c_str(), &end, 10));
  if (end == str.c_str() + str.size()) {
    return result;
  }
  return none;
}

// converts "+" to the atom '+' and "-" to the atom '-'
optional<atom_value> plus_or_minus(const string& str) {
  if (str == "+")
    return plus_atom::value;
  if (str == "-")
    return minus_atom::value;
  return none;
}

void client_repl(actor_system& system, string host, uint16_t port) {
  // keeps track of requests and tries to reconnect on server failures
  auto usage = [] {
  cout << "Usage:" << endl
       << "  quit                  : terminates the program" << endl
       << "  connect <host> <port> : connects to a remote actor" << endl
       << "  <x> + <y>             : adds two integers" << endl
       << "  <x> - <y>             : subtracts two integers" << endl
       << endl;
  };
  usage();
  bool done = false;
  auto client = system.spawn<client_impl>(std::move(host), port);
  // defining the handler outside the loop is more efficient as it avoids
  // re-creating the same object over and over again
  message_handler eval{
    [&](const string& cmd) {
      if (cmd != "quit")
        return;
      anon_send_exit(client, exit_reason::user_shutdown);
      done = true;
    },
    [&](string& arg0, string& arg1, string& arg2) {
      if (arg0 == "connect") {
        try {
          auto lport = std::stoul(arg2);
          if (lport < std::numeric_limits<uint16_t>::max()) {
            anon_send(client, rebind_atom::value, move(arg1),
              static_cast<uint16_t>(lport));
          }
          else {
            cout << lport << " is not a valid port" << endl;
          }
        }
        catch (std::exception&) {
          cout << "\"" << arg2 << "\" is not an unsigned integer"
            << endl;
        }
      }
      else {
        auto x = toint(arg0);
        auto op = plus_or_minus(arg1);
        auto y = toint(arg2);
        if (x && y && op)
          anon_send(client, *op, *x, *y);
      }
    }
  };
  // read next line, split it, and feed to the eval handler
  string line;
  while (! done && std::getline(std::cin, line)) {
    line = trim(std::move(line)); // ignore leading and trailing whitespaces
    std::vector<string> words;
    split(words, line, is_any_of(" "), token_compress_on);
    if (! message_builder(words.begin(), words.end()).apply(eval))
      usage();
  }
}

class config : public actor_system_config {
public:
  uint16_t port = 0;
  std::string host = "localhost";
  bool server_mode = false;

  config() {
    opt_group{custom_options_, "global"}
    .add(port, "port,p", "set port")
    .add(host, "host,H", "set host (ignored in server mode)")
    .add(server_mode, "server-mode,s", "enable server mode");
  }
};

void caf_main(actor_system& system, const config& cfg) {
  if (! cfg.server_mode && cfg.port == 0) {
    cerr << "*** no port to server specified" << endl;
    return;
  }
  if (cfg.server_mode) {
    auto calc = system.spawn(calculator_fun);
    // try to publish math actor at given port
    cout << "*** try publish at port " << cfg.port << endl;
    auto expected_port = system.middleman().publish(calc, cfg.port);
    if (! expected_port) {
      std::cerr << "*** publish failed: "
                << system.render(expected_port.error()) << endl;
    } else {
      cout << "*** server successfully published at port " << *expected_port
           << endl  << "*** press [enter] to quit" << endl;
      string dummy;
      std::getline(std::cin, dummy);
      cout << "... cya" << endl;
      anon_send_exit(calc, exit_reason::user_shutdown);
    }
    return;
  }
  client_repl(system, cfg.host, cfg.port);
}

} // namespace <anonymous>

CAF_MAIN(io::middleman)
