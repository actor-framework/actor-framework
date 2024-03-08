// This program is a distributed version of the math_actor example. Client and
// server use a stateless request/response protocol and the client is failure
// resilient by using a FIFO request queue. The client auto-reconnects and also
// allows for server reconfiguration.
//
// Run server at port 4242:
// - distributed_calculator -s -p 4242
//
// Run client at the same host:
// - distributed_calculator -p 4242

#include "caf/io/middleman.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/anon_mail.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/message_builder.hpp"

#include <array>
#include <cassert>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::string;

using namespace caf;

namespace {

constexpr auto task_timeout = std::chrono::seconds(10);

// our "service"
behavior calculator_fun() {
  return {
    [](add_atom, int a, int b) { return a + b; },
    [](sub_atom, int a, int b) { return a - b; },
  };
}

// State transition of the client for connecting to the server:
//
//                    +-------------+
//                    |    init     |
//                    +-------------+
//                           |
//                           V
//                    +-------------+
//                    | unconnected |<------------------+
//                    +-------------+                   |
//                           |                          |
//                           | {connect Host Port}      |
//                           |                          |
//                           V                          |
//                    +-------------+  {error}          |
//    +-------------->| connecting  |-------------------+
//    |               +-------------+                   |
//    |                      |                          |
//    |                      | {ok, Calculator}         |
//    |{connect Host Port}   |                          |
//    |                      V                          |
//    |               +-------------+   {DOWN server}   |
//    +---------------|   running   |-------------------+
//                    +-------------+

// a simple calculator task: operation + operands
struct task {
  std::variant<add_atom, sub_atom> op;
  int lhs;
  int rhs;
};

// the client queues pending tasks
struct client_state {
  explicit client_state(event_based_actor* selfptr) : self(selfptr) {
    // nop
  }

  behavior make_behavior() {
    return unconnected();
  }

  behavior unconnected() {
    return {
      [this](add_atom op, int x, int y) {
        tasks.emplace_back(task{op, x, y});
      },
      [this](sub_atom op, int x, int y) {
        tasks.emplace_back(task{op, x, y});
      },
      [this](connect_atom, const std::string& host, uint16_t port) {
        connecting(host, port);
      },
    };
  }

  void connecting(const std::string& host, uint16_t port) {
    // make sure we are not pointing to an old server
    current_server = nullptr;
    // use request().await() to suspend regular behavior until MM responded
    auto mm = self->system().middleman().actor_handle();
    self->mail(connect_atom_v, host, port)
      .request(mm, infinite)
      .await(
        [this, host, port](const node_id&, strong_actor_ptr serv,
                           const std::set<std::string>& ifs) {
          if (!serv) {
            self->println("*** no server found at {}:{}", host, port);
            return;
          }
          if (!ifs.empty()) {
            self->println(
              "*** typed actor found at {}:{}, but expected an untyped actor",
              host, port);
            return;
          }
          self->println("*** successfully connected to server");
          current_server = serv;
          auto hdl = actor_cast<actor>(serv);
          self->monitor(hdl, [this](const error&) {
            // Transition to `unconnected` on server failure.
            self->println("*** lost connection to server");
            current_server = nullptr;
            self->become(unconnected());
          });
          self->become(running(hdl));
        },
        [this, host, port](const error& err) {
          self->println("*** cannot connect to {}:{} => {}", host, port, err);
          self->become(unconnected());
        });
  }

  behavior running(const actor& calculator) {
    auto send_task = [this, calculator](auto op, int x, int y) {
      self->mail(op, x, y)
        .request(calculator, task_timeout)
        .then(
          [this, x, y](int result) {
            char op_ch;
            if constexpr (std::is_same_v<add_atom, decltype(op)>)
              op_ch = '+';
            else
              op_ch = '-';
            self->println("{} {} {} = {}", x, op_ch, y, result);
          },
          [this, op, x, y](const error&) {
            // simply try again by enqueueing the task to the mailbox again
            self->mail(op, x, y).send(self);
          });
    };
    for (auto& x : tasks) {
      auto f = [&](auto op) { send_task(op, x.lhs, x.rhs); };
      std::visit(f, x.op);
    }
    tasks.clear();
    return {
      [send_task](add_atom op, int x, int y) { send_task(op, x, y); },
      [send_task](sub_atom op, int x, int y) { send_task(op, x, y); },
      [this](connect_atom, const std::string& host, uint16_t port) {
        connecting(host, port);
      },
    };
  }

  event_based_actor* self;
  strong_actor_ptr current_server;
  std::vector<task> tasks;
};

// removes leading and trailing whitespaces
string trim(std::string s) {
  auto not_space = [](char c) { return isspace(c) == 0; };
  // trim left
  s.erase(s.begin(), find_if(s.begin(), s.end(), not_space));
  // trim right
  s.erase(find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

// tries to convert `str` to an int
std::optional<int> toint(const string& str) {
  char* end;
  auto result = static_cast<int>(strtol(str.c_str(), &end, 10));
  if (end == str.c_str() + str.size())
    return result;
  return std::nullopt;
}

// --(rst-config-begin)--
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
// --(rst-config-end)--

void client_repl(actor_system& sys, const config& cfg) {
  // keeps track of requests and tries to reconnect on server failures
  auto usage = [&sys] {
    sys.println("Usage:");
    sys.println("  quit                  : terminates the program");
    sys.println("  connect <host> <port> : connects to a remote actor");
    sys.println("  <x> + <y>             : adds two integers");
    sys.println("  <x> - <y>             : subtracts two integers");
    sys.println("");
  };
  usage();
  bool done = false;
  auto client = sys.spawn(actor_from_state<client_state>);
  if (!cfg.host.empty() && cfg.port > 0)
    anon_mail(connect_atom_v, cfg.host, cfg.port).send(client);
  else
    sys.println("*** no server received via config, please set one via "
                "'connect <host> <port>' before using the calculator");
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
        char* end = nullptr;
        auto lport = strtoul(arg2.c_str(), &end, 10);
        if (end != arg2.c_str() + arg2.size())
          sys.println("'{}' is not an unsigned integer", arg2);
        else if (lport > std::numeric_limits<uint16_t>::max())
          sys.println("{} > {}", arg2, std::numeric_limits<uint16_t>::max());
        else
          anon_mail(connect_atom_v, std::move(arg1),
                    static_cast<uint16_t>(lport))
            .send(client);
      } else {
        auto x = toint(arg0);
        auto y = toint(arg2);
        if (x && y) {
          if (arg1 == "+")
            anon_mail(add_atom_v, *x, *y).send(client);
          else if (arg1 == "-")
            anon_mail(sub_atom_v, *x, *y).send(client);
        }
      }
    }};
  // read next line, split it, and feed to the eval handler
  string line;
  while (!done && std::getline(std::cin, line)) {
    line = trim(std::move(line)); // ignore leading and trailing whitespaces
    std::vector<string> words;
    split(words, line, is_any_of(" "), token_compress_on);
    auto msg = message_builder(words.begin(), words.end()).move_to_message();
    if (!eval(msg))
      usage();
  }
}

void run_server(actor_system& sys, const config& cfg) {
  auto calc = sys.spawn(calculator_fun);
  // try to publish math actor at given port
  sys.println("*** try publish at port {}", cfg.port);
  auto expected_port = sys.middleman().publish(calc, cfg.port);
  if (!expected_port) {
    sys.println("*** to publish the calculator: {}", expected_port.error());
    return;
  }
  sys.println("*** server successfully published at port {}", *expected_port);
  sys.println("*** press [enter] to quit");
  string dummy;
  std::getline(std::cin, dummy);
  sys.println("*** shutting down");
  anon_send_exit(calc, exit_reason::user_shutdown);
}

void caf_main(actor_system& sys, const config& cfg) {
  auto f = cfg.server_mode ? run_server : client_repl;
  f(sys, cfg);
}

} // namespace

CAF_MAIN(io::middleman)
