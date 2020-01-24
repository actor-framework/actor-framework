/******************************************************************************\
 * This example program represents a minimal terminal chat program            *
 * based on group communication.                                              *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - ./build/bin/group_chat -s -p 4242                                        *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n alice        *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n bob          *
\******************************************************************************/

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/string_algorithms.hpp"

using namespace std;
using namespace caf;

namespace {

CAF_MSG_TYPE_ADD_ATOM(broadcast_atom);

struct line {
  string str;
};

istream& operator>>(istream& is, line& l) {
  getline(is, l.str);
  return is;
}

behavior client(event_based_actor* self, const string& name) {
  return {
    [=](broadcast_atom, const string& message) {
      for (auto& dest : self->joined_groups()) {
        self->send(dest, name + ": " + message);
      }
    },
    [=](join_atom, const group& what) {
      for (const auto& g : self->joined_groups()) {
        cout << "*** leave " << to_string(g) << endl;
        self->send(g, name + " has left the chatroom");
        self->leave(g);
      }
      cout << "*** join " << to_string(what) << endl;
      self->join(what);
      self->send(what, name + " has entered the chatroom");
    },
    [=](const string& txt) {
      // don't print own messages
      if (self->current_sender() != self)
        cout << txt << endl;
    },
    [=](const group_down_msg& g) {
      cout << "*** chatroom offline: " << to_string(g.source) << endl;
    },
  };
}

class config : public actor_system_config {
public:
  std::string name;
  std::vector<std::string> group_uris;
  uint16_t port = 0;
  bool server_mode = false;

  config() {
    opt_group{custom_options_, "global"}
      .add(name, "name,n", "set name")
      .add(group_uris, "group,g", "join group")
      .add(server_mode, "server,s", "run in server mode")
      .add(port, "port,p", "set port (ignored in client mode)");
  }
};

void run_server(actor_system& system, const config& cfg) {
  auto res = system.middleman().publish_local_groups(cfg.port);
  if (!res) {
    std::cerr << "*** publishing local groups failed: "
              << system.render(res.error()) << endl;
    return;
  }
  cout << "*** listening at port " << *res << endl
       << "*** press [enter] to quit" << endl;
  string dummy;
  std::getline(std::cin, dummy);
  cout << "... cya" << endl;
}

void run_client(actor_system& system, const config& cfg) {
  auto name = cfg.name;
  while (name.empty()) {
    cout << "please enter your name: " << flush;
    if (!getline(cin, name)) {
      cerr << "*** no name given... terminating" << endl;
      return;
    }
  }
  cout << "*** starting client, type '/help' for a list of commands" << endl;
  auto client_actor = system.spawn(client, name);
  for (auto& uri : cfg.group_uris) {
    auto tmp = system.groups().get(uri);
    if (tmp)
      anon_send(client_actor, join_atom_v, std::move(*tmp));
    else
      cerr << R"(*** failed to parse ")" << uri << R"(" as group URI: )"
           << system.render(tmp.error()) << endl;
  }
  istream_iterator<line> eof;
  vector<string> words;
  for (istream_iterator<line> i(cin); i != eof; ++i) {
    auto send_input = [&] {
      if (!i->str.empty())
        anon_send(client_actor, broadcast_atom_v, i->str);
    };
    words.clear();
    split(words, i->str, is_any_of(" "));
    auto res
      = message_builder(words.begin(), words.end())
          .apply(
            {[&](const string& cmd, const string& mod, const string& id) {
               if (cmd == "/join") {
                 auto grp = system.groups().get(mod, id);
                 if (grp)
                   anon_send(client_actor, join_atom_v, *grp);
               } else {
                 send_input();
               }
             },
             [&](const string& cmd) {
               if (cmd == "/quit") {
                 cin.setstate(ios_base::eofbit);
               } else if (cmd[0] == '/') {
                 cout << "*** available commands:\n"
                         "  /join <module> <group> join a new chat channel\n"
                         "  /quit          quit the program\n"
                         "  /help          print this text\n"
                      << flush;
               } else {
                 send_input();
               }
             }});
    if (!res)
      send_input();
  }
  // force actor to quit
  anon_send_exit(client_actor, exit_reason::user_shutdown);
}

void caf_main(actor_system& system, const config& cfg) {
  auto f = cfg.server_mode ? run_server : run_client;
  f(system, cfg);
}

} // namespace

CAF_MAIN(io::middleman)
