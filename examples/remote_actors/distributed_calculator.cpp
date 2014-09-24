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

#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <iostream>
#include <functional>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

// the options_description API is deprecated
#include "cppa/opt.hpp"

using namespace std;
using namespace caf;

// our "service"
void calculator(event_based_actor* self) {
  self->become (
    on(atom("plus"), arg_match) >> [](int a, int b) -> message {
      return make_message(atom("result"), a + b);
    },
    on(atom("minus"), arg_match) >> [](int a, int b) -> message {
      return make_message(atom("result"), a - b);
    },
    on(atom("quit")) >> [=] {
      self->quit();
    }
  );
}

inline string trim(std::string s) {
  auto not_space = [](char c) { return !isspace(c); };
  // trim left
  s.erase(s.begin(), find_if(s.begin(), s.end(), not_space));
  // trim right
  s.erase(find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

void client_bhvr(event_based_actor* self, const string& host,
                 uint16_t port, const actor& server) {
  // recover from sync failures by trying to reconnect to server
  if (!self->has_sync_failure_handler()) {
    self->on_sync_failure([=] {
      aout(self) << "*** lost connection to " << host
             << ":" << port << endl;
      client_bhvr(self, host, port, invalid_actor);
    });
  }
  // connect to server if needed
  if (!server) {
    aout(self) << "*** try to connect to " << host << ":" << port << endl;
    try {
      auto new_serv = io::remote_actor(host, port);
      self->monitor(new_serv);
      aout(self) << "reconnection succeeded" << endl;
      client_bhvr(self, host, port, new_serv);
      return;
    }
    catch (exception&) {
      aout(self) << "connection failed, try again in 3s" << endl;
      self->delayed_send(self, chrono::seconds(3), atom("reconnect"));
    }
  }
  // our predicate guarding the first callback
  auto pred = [=](atom_value val) -> optional<atom_value> {
    if (server != invalid_actor
        && (val == atom("plus") || val == atom("minus"))) {
      return val;
    }
    return none;
  };
  self->become (
    on(pred, val<int>, val<int>) >> [=](atom_value op, int lhs, int rhs) {
      self->sync_send_tuple(server, self->last_dequeued()).then(
        on(atom("result"), arg_match) >> [=](int result) {
          aout(self) << lhs << " "
                 << to_string(op) << " "
                 << rhs << " = "
                 << result << endl;
        }
      );
    },
    [=](const down_msg&) {
      aout(self) << "*** server down, try to reconnect ..." << endl;
      client_bhvr(self, host, port, invalid_actor);
    },
    on(atom("rebind"), arg_match) >> [=](const string& nhost, uint16_t nport) {
      aout(self) << "*** rebind to new server: "
             << nhost << ":" << nport << endl;
      client_bhvr(self, nhost, nport, invalid_actor);
    },
    on(atom("reconnect")) >> [=] {
      client_bhvr(self, host, port, invalid_actor);
    }
  );
}

void client_repl(const string& host, uint16_t port) {
  // keeps track of requests and tries to reconnect on server failures
  cout << "Usage:\n"
          "quit                   Quit the program\n"
          "<x> + <y>              Calculate <x>+<y> and print result\n"
          "<x> - <y>              Calculate <x>-<y> and print result\n"
          "connect <host> <port>  Reconfigure server"
       << endl << endl;
  string line;
  auto client = spawn(client_bhvr, host, port, invalid_actor);
  const char connect[] = "connect ";
  while (getline(cin, line)) {
    line = trim(std::move(line)); // ignore leading and trailing whitespaces
    if (line == "quit") {
      // force client to quit
      anon_send_exit(client, exit_reason::user_shutdown);
      return;
    } else if (equal(begin(connect), end(connect) - 1, begin(line))) {
      match_split(line, ' ') (
        on("connect", arg_match) >> [&](string& nhost, string& sport) {
          try {
            auto lport = std::stoul(sport);
            if (lport < std::numeric_limits<uint16_t>::max()) {
              anon_send(client, atom("rebind"), move(nhost),
                    static_cast<uint16_t>(lport));
            }
            else {
              cout << lport << " is not a valid port" << endl;
            }
          }
          catch (std::exception&) {
            cout << "\"" << sport << "\" is not an unsigned integer"
               << endl;
          }
        },
        others() >> [] {
          cout << "*** usage: connect <host> <port>" << endl;
        }
      );
    } else {
      auto toint = [](const string& str) -> optional<int> {
        try { return {std::stoi(str)}; }
        catch (std::exception&) {
          cout << "\"" << str << "\" is not an integer" << endl;
          return none;
        }
      };
      bool success = false;
      auto first = begin(line);
      auto last = end(line);
      auto pos = find_if(first, last, [](char c) { return c == '+' || c == '-'; });
      if (pos != last) {
        auto lsub = trim(string(first, pos));
        auto rsub = trim(string(pos + 1, last));
        auto lhs = toint(lsub);
        auto rhs = toint(rsub);
        if (lhs && rhs) {
          auto op = (*pos == '+') ? atom("plus") : atom("minus");
          anon_send(client, op, *lhs, *rhs);
        }
      }
      else if (!success) {
        cout << "*** invalid format; usage: <x> [+|-] <y>" << endl;
      }
    }
  }
}

int main(int argc, char** argv) {
  string mode;
  string host;
  uint16_t port = 0;
  options_description desc;
  auto set_mode = [&](const string& arg) -> function<bool()> {
    return [arg, &mode]() -> bool {
      if (!mode.empty()) {
        cerr << "mode already set to " << mode << endl;
        return false;
      }
      mode = move(arg);
      return true;
    };
  };
  string copts = "client options";
  string sopts = "server options";
  bool args_valid = match_stream<string> (argv + 1, argv + argc) (
    on_opt1('p', "port",   &desc, "set port") >> rd_arg(port),
    on_opt1('H', "host",   &desc, "set host (default: localhost)", copts) >> rd_arg(host),
    on_opt0('s', "server", &desc, "run in server mode", sopts) >> set_mode("server"),
    on_opt0('c', "client", &desc, "run in client mode", copts) >> set_mode("client"),
    on_opt0('h', "help",   &desc, "print help") >> print_desc_and_exit(&desc)
  );
  if (!args_valid || port == 0 || mode.empty()) {
    if (port == 0) cerr << "*** no port specified" << endl;
    if (mode.empty()) cerr << "*** no mode specified" << endl;
    cerr << endl;
    auto description_printer = print_desc(&desc, cerr);
    description_printer();
    return -1;
  }
  if (mode == "server") {
    try {
      // try to publish math actor at given port
      io::publish(spawn(calculator), port);
    }
    catch (exception& e) {
      cerr << "*** unable to publish math actor at port " << port << "\n"
         << to_verbose_string(e) // prints exception type and e.what()
         << endl;
    }
  }
  else {
    if (host.empty()) host = "localhost";
    client_repl(host, port);
  }
  await_all_actors_done();
  shutdown();
  return 0;
}
