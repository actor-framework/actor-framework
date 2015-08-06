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
    },
    others >> [=]() {
      cout << "unexpected: " << to_string(self->current_message()) << endl;
    }
  );
}

int main(int argc, char** argv) {
  string name;
  string group_id;
  auto res = message_builder(argv + 1, argv + argc).extract_opts({
    {"name,n", "set name", name},
    {"group,g", "join group", group_id}
  });
  if (! res.error.empty()) {
    cerr << res.error << endl;
    return 1;
  }
  if (! res.remainder.empty()) {
    std::cout << res.helptext << std::endl;
    return 1;
  }
  if (res.opts.count("help") > 0) {
    cout << res.helptext << endl;
    return 0;
  }
  while (name.empty()) {
    cout << "please enter your name: " << flush;
    if (! getline(cin, name)) {
      cerr << "*** no name given... terminating" << endl;
      return 1;
    }
  }
  auto client_actor = spawn(client, name);
  // evaluate group parameters
  if (! group_id.empty()) {
    auto p = group_id.find(':');
    if (p == std::string::npos) {
      cerr << "*** error parsing argument " << group_id
         << ", expected format: <module_name>:<group_id>";
    } else {
      try {
        auto module = group_id.substr(0, p);
        auto group_uri = group_id.substr(p + 1);
        auto g = (module == "remote") ? io::remote_group(group_uri)
                                      : group::get(module, group_uri);
        anon_send(client_actor, join_atom::value, g);
      }
      catch (exception& e) {
        cerr << "*** exception: group::get(\"" << group_id.substr(0, p)
           << "\", \"" << group_id.substr(p + 1) << "\") failed; "
           << to_verbose_string(e) << endl;
      }
    }
  }
  cout << "*** starting client, type '/help' for a list of commands" << endl;
  istream_iterator<line> eof;
  vector<string> words;
  for (istream_iterator<line> i(cin); i != eof; ++i) {
    auto send_input = [&] {
      if (!i->str.empty()) {
        anon_send(client_actor, broadcast_atom::value, i->str);
      }
    };
    words.clear();
    split(words, i->str, is_any_of(" "));
    message_builder(words.begin(), words.end()).apply({
      [&](const string& cmd, const string& mod, const string& id) {
        if (cmd == "/join") {
          try {
            group grp = (mod == "remote") ? io::remote_group(id)
              : group::get(mod, id);
            anon_send(client_actor, join_atom::value, grp);
          }
          catch (exception& e) {
            cerr << "*** exception: " << to_verbose_string(e) << endl;
          }
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
      },
      others >> send_input
    });
  }
  // force actor to quit
  anon_send_exit(client_actor, exit_reason::user_shutdown);
  await_all_actors_done();
  shutdown();
}
