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

using namespace std;
using namespace caf;

using plus_atom = atom_constant<atom("plus")>;
using minus_atom = atom_constant<atom("minus")>;
using result_atom = atom_constant<atom("result")>;
using rebind_atom = atom_constant<atom("rebind")>;
using connect_atom = atom_constant<atom("connect")>;
using reconnect_atom = atom_constant<atom("reconnect")>;

// our "service"
void calculator(event_based_actor* self) {
  self->become (
    [](plus_atom, int a, int b) -> message {
      return make_message(result_atom::value, a + b);
    },
    [](minus_atom, int a, int b) -> message {
      return make_message(result_atom::value, a - b);
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
      self->delayed_send(self, chrono::seconds(3), reconnect_atom::value);
    }
  }
  auto sync_send_request = [=](int lhs, const char* op, int rhs) {
    self->sync_send(server, self->current_message()).then(
      [=](result_atom, int result) {
        aout(self) << lhs << " " << op << " " << rhs << " = " << result << endl;
      }
    );
  };
  self->become (
    [=](plus_atom, int lhs, int rhs) {
      sync_send_request(lhs, "+", rhs);
    },
    [=](minus_atom, int lhs, int rhs) {
      sync_send_request(lhs, "-", rhs);
    },
    [=](const down_msg&) {
      aout(self) << "*** server down, try to reconnect ..." << endl;
      client_bhvr(self, host, port, invalid_actor);
    },
    [=](rebind_atom, const string& nhost, uint16_t nport) {
      aout(self) << "*** rebind to new server: "
             << nhost << ":" << nport << endl;
      client_bhvr(self, nhost, nport, invalid_actor);
    },
    [=](rebind_atom) {
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
      vector<string> words;
      split(words, line, is_any_of(" "));
      message_builder(words.begin(), words.end()).apply({
        on("connect", arg_match) >> [&](string& nhost, string& sport) {
          try {
            auto lport = std::stoul(sport);
            if (lport < std::numeric_limits<uint16_t>::max()) {
              anon_send(client, rebind_atom::value, move(nhost),
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
      });
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
          atom_value op;
          if (*pos == '+') {
            op = plus_atom::value;
          } else {
            op = minus_atom::value;
          }
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
  uint16_t port = 0;
  string host = "localhost";
  auto res = message_builder(argv + 1, argv + argc).filter_cli({
    {"port,p", "set port (either to publish at or to connect to)", port},
    {"host,H", "set host (client mode only, default: localhost)", host},
    {"server,s", "run in server mode"},
    {"client,c", "run in client mode"}
  });
  if (res.opts.count("help") > 0) {
    return 0;
  }
  if (!res.remainder.empty()) {
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
  if (!is_server && port == 0) {
    cerr << "*** no port to server specified" << endl;
    return 1;
  }
  if (is_server) {
    try {
      // try to publish math actor at given port
      cout << "*** try publish at port " << port << endl;
      auto p = io::publish(spawn(calculator), port);
      cout << "*** server successfully published at port " << p << endl;
    }
    catch (exception& e) {
      cerr << "*** unable to publish math actor at port " << port << "\n"
         << to_verbose_string(e) // prints exception type and e.what()
         << endl;
    }
  }
  else {
    client_repl(host, port);
  }
  await_all_actors_done();
  shutdown();
}
