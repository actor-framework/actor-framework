/******************************************************************************\
 * This example program represents a minimal terminal chat program            *
 * based on group communication.                                              *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - ./build/bin/group_server -p 4242                                         *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n alice        *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n bob          *
\ ******************************************************************************/

#include <set>
#include <map>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <iostream>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/string_algorithms.hpp"

using namespace std;
using namespace caf;

namespace {

using broadcast_atom = atom_constant<atom("broadcast")>;

struct line { string str; };

istream& operator>>(istream& is, line& l) {
  getline(is, l.str);
  return is;
}

void client(event_based_actor* self, const string& name) {
  self->become (
    [=](broadcast_atom, const string& message) {
      for(auto& dest : self->joined_groups()) {
        self->send(dest, name + ": " + message);
      }
    },
    [=](join_atom, const group& what) {
      for (auto g : self->joined_groups()) {
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
      if (self->current_sender() != self) {
        cout << txt << endl;
      }
    },
    [=](const group_down_msg& g) {
      cout << "*** chatroom offline: " << to_string(g.source) << endl;
    }
  );
}

class config : public actor_system_config {
public:
  std::string name;
  std::string group_id;

  config() {
    opt_group{custom_options_, "global"}
    .add(name, "name,n", "set name")
    .add(group_id, "group,g", "join group");
  }
};

void caf_main(actor_system& system, const config& cfg) {
  auto name = cfg.name;
  while (name.empty()) {
    cout << "please enter your name: " << flush;
    if (! getline(cin, name)) {
      cerr << "*** no name given... terminating" << endl;
      return;
    }
  }
  auto client_actor = system.spawn(client, name);
  // evaluate group parameters
  if (! cfg.group_id.empty()) {
    auto p = cfg.group_id.find(':');
    if (p == std::string::npos) {
      cerr << "*** error parsing argument " << cfg.group_id
         << ", expected format: <module_name>:<group_id>";
    } else {
      auto module = cfg.group_id.substr(0, p);
      auto group_uri = cfg.group_id.substr(p + 1);
      auto g = system.groups().get(module, group_uri);
      if (! g) {
        cerr << "*** unable to get group " << group_uri
             << " from module " << module << ": "
             << system.render(g.error()) << endl;
        return;
      }
      anon_send(client_actor, join_atom::value, *g);
    }
  }
  cout << "*** starting client, type '/help' for a list of commands" << endl;
  istream_iterator<line> eof;
  vector<string> words;
  for (istream_iterator<line> i(cin); i != eof; ++i) {
    auto send_input = [&] {
      if (! i->str.empty())
        anon_send(client_actor, broadcast_atom::value, i->str);
    };
    words.clear();
    split(words, i->str, is_any_of(" "));
    auto res = message_builder(words.begin(), words.end()).apply({
      [&](const string& cmd, const string& mod, const string& id) {
        if (cmd == "/join") {
          auto grp = system.groups().get(mod, id);
          if (grp)
            anon_send(client_actor, join_atom::value, *grp);
        }
        else {
          send_input();
        }
      },
      [&](const string& cmd) {
        if (cmd == "/quit") {
          cin.setstate(ios_base::eofbit);
        }
        else if (cmd[0] == '/') {
          cout << "*** available commands:\n"
            "  /join <module> <group> join a new chat channel\n"
            "  /quit          quit the program\n"
            "  /help          print this text\n" << flush;
        }
        else {
          send_input();
        }
      }
    });
    if (! res)
      send_input();
  }
  // force actor to quit
  anon_send_exit(client_actor, exit_reason::user_shutdown);
}

} // namespace <anonymous>

CAF_MAIN(io::middleman)
