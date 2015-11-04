/******************************************************************************\
 * This program is a distributed version of the math_actor example.           *
 * Client and server use a stateless request/response protocol and the client *
 * is failure resilient by using a FIFO request queue.                        *
 * The client auto-reconnects and also allows for server reconfiguration.     *
 *                                                                            *
 * Run server at port 4242:                                                   *
 * - ./build/bin/distributed_math_actor -s -p 4242                            *
 *                                                                            *
 * Run client at the same host:                                               *
 * - ./build/bin/distributed_math_actor -c -p 4242                            *
\ ******************************************************************************/

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
behavior calculator() {
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
  client_impl(string hostaddr, uint16_t port)
      : host_(std::move(hostaddr)),
        port_(port) {
    // nop
  }

  behavior make_behavior() override {
    become(awaiting_task());
    become(keep_behavior, reconnecting());
    return {};
  }

private:
  void sync_send_task(atom_value op, int lhs, int rhs) {
    on_sync_failure([=] {
      aout(this) << "*** sync_failure!" << endl;
    });
    sync_send(server_, op, lhs, rhs).then(
      [=](result_atom, int result) {
        aout(this) << lhs << (op == plus_atom::value ? " + " : " - ")
                   << rhs << " = " << result << endl;
      },
      [=](const sync_exited_msg& msg) {
        aout(this) << "*** server down [" << msg.reason << "], "
                   << "try to reconnect ..." << endl;
        // try sync_sending this again after successful reconnect
        become(keep_behavior,
               reconnecting([=] { sync_send_task(op, lhs, rhs); }));
      }
    );
  }

  behavior awaiting_task() {
    return {
      [=](atom_value op, int lhs, int rhs) {
        if (op != plus_atom::value && op != minus_atom::value) {
          return;
        }
        sync_send_task(op, lhs, rhs);
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
    auto mm = io::get_middleman_actor();
    send(mm, connect_atom::value, host_, port_);
    return {
      [=](ok_atom, node_id&, actor_addr& new_server, std::set<std::string>&) {
        if (new_server == invalid_actor_addr) {
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
      [=](error_atom, const string& errstr) {
        aout(this) << "*** could not connect to " << host_
                   << " at port " << port_
                   << ": " << errstr
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
          [=](error_atom&, string&)  {
            send_mm();
          }
        );
      },
      // simply ignore all requests until we have a connection
      others >> skip_message
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
maybe<int> toint(const string& str) {
  char* end;
  auto result = static_cast<int>(strtol(str.c_str(), &end, 10));
  if (end == str.c_str() + str.size()) {
    return result;
  }
  return none;
}

// converts "+" to the atom '+' and "-" to the atom '-'
maybe<atom_value> plus_or_minus(const string& str) {
  if (str == "+") {
    return maybe<atom_value>{plus_atom::value};
  }
  if (str == "-") {
    return maybe<atom_value>{minus_atom::value};
  }
  return none;
}

void client_repl(string host, uint16_t port) {
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
  auto client = spawn<client_impl>(std::move(host), port);
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
    },
    others >> usage
  };
  // read next line, split it, and feed to the eval handler
  string line;
  while (! done && std::getline(std::cin, line)) {
    line = trim(std::move(line)); // ignore leading and trailing whitespaces
    std::vector<string> words;
    split(words, line, is_any_of(" "), token_compress_on);
    message_builder(words.begin(), words.end()).apply(eval);
  }
}

} // namespace <anonymous>

int main(int argc, char** argv) {
  uint16_t port = 0;
  string host = "localhost";
  auto res = message_builder(argv + 1, argv + argc).extract_opts({
    {"port,p", "set port (either to publish at or to connect to)", port},
    {"host,H", "set host (client mode only, default: localhost)", host},
    {"server,s", "run in server mode"},
    {"client,c", "run in client mode"}
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
    cerr << "*** invalid command line options" << endl << res.helptext << endl;
    return 1;
  }
  bool is_server = res.opts.count("server") > 0;
  if (is_server == (res.opts.count("client") > 0)) {
    if (is_server) {
      cerr << "*** cannot be server and client at the same time" << endl;
    } else {
      cerr << "*** either --server or --client option must be set" << endl;
    }
    return 1;
  }
  if (! is_server && port == 0) {
    cerr << "*** no port to server specified" << endl;
    return 1;
  }
  if (is_server) {
    auto calc = spawn(calculator);
    try {
      // try to publish math actor at given port
      cout << "*** try publish at port " << port << endl;
      auto p = io::publish(calc, port);
      cout << "*** server successfully published at port " << p << endl;
      cout << "*** press [enter] to quit" << endl;
      string dummy;
      std::getline(std::cin, dummy);
      cout << "... cya" << endl;
    }
    catch (std::exception& e) {
      cerr << "*** unable to publish math actor at port " << port << "\n"
         << to_verbose_string(e) // prints exception type and e.what()
         << endl;
    }
    anon_send_exit(calc, exit_reason::user_shutdown);
  }
  else {
    client_repl(host, port);
  }
  await_all_actors_done();
  shutdown();
}
