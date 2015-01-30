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

using join_atom = atom_constant<atom("join")>;
using broadcast_atom = atom_constant<atom("broadcast")>;

struct line { string str; };

istream& operator>>(istream& is, line& l) {
  getline(is, l.str);
  return is;
}

namespace { string s_last_line; }

message split_line(const line& l) {
  istringstream strs(l.str);
  s_last_line = move(l.str);
  string tmp;
  message_builder mb;
  while (getline(strs, tmp, ' ')) {
    if (!tmp.empty()) mb.append(std::move(tmp));
  }
  return mb.to_message();
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
        self->send(self, g, name + " has left the chatroom");
        self->leave(g);
      }
      cout << "*** join " << to_string(what) << endl;
      self->join(what);
      self->send(what, name + " has entered the chatroom");
    },
    [=](const string& txt) {
      // don't print own messages
      if (self->last_sender() != self) cout << txt << endl;
    },
    [=](const group_down_msg& g) {
      cout << "*** chatroom offline: " << to_string(g.source) << endl;
    },
    others() >> [=]() {
      cout << "unexpected: " << to_string(self->last_dequeued()) << endl;
    }
  );
}

int main(int argc, char** argv) {
  string name;
  string group_id;
  auto res = message_builder(argv + 1, argv + argc).filter_cli({
    {"name,n", "set name", name},
    {"group,g", "join group", group_id}
  });
  if (!res.remainder.empty()) {
    std::cout << res.helptext << std::endl;
    return 1;
  }
  if (res.opts.count("help") > 0) {
    return 0;
  }
  while (name.empty()) {
    cout << "please enter your name: " << flush;
    if (!getline(cin, name)) {
      cerr << "*** no name given... terminating" << endl;
      return 1;
    }
  }
  auto client_actor = spawn(client, name);
  // evaluate group parameters
  if (!group_id.empty()) {
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
        ostringstream err;
        cerr << "*** exception: group::get(\"" << group_id.substr(0, p)
           << "\", \"" << group_id.substr(p + 1) << "\") failed; "
           << to_verbose_string(e) << endl;
      }
    }
  }
  cout << "*** starting client, type '/help' for a list of commands" << endl;
  auto starts_with = [](const string& str) -> function<optional<string> (const string&)> {
    return [=](const string& arg) -> optional<string> {
      if (arg.compare(0, str.size(), str) == 0) {
        return arg;
      }
      return none;
    };
  };
  istream_iterator<line> eof;
  vector<string> words;
  for (istream_iterator<line> i(cin); i != eof; ++i) {
    words.clear();
    split(words, i->str, is_any_of(" "));
    message_builder(words.begin(), words.end()).apply({
      on("/join", arg_match) >> [&](const string& mod, const string& id) {
        try {
          group grp = (mod == "remote") ? io::remote_group(id)
                                        : group::get(mod, id);
          anon_send(client_actor, join_atom::value, grp);
        }
        catch (exception& e) {
          cerr << "*** exception: " << to_verbose_string(e) << endl;
        }
      },
      on("/quit") >> [&] {
        // close STDIN; causes this match loop to quit
        cin.setstate(ios_base::eofbit);
      },
      on(starts_with("/"), any_vals) >> [&] {
        cout <<  "*** available commands:\n"
             "  /join <module> <group> join a new chat channel\n"
             "  /quit          quit the program\n"
             "  /help          print this text\n" << flush;
      },
      others() >> [&] {
        if (!s_last_line.empty()) {
          anon_send(client_actor, broadcast_atom::value, s_last_line);
        }
      }
    });
  }
  // force actor to quit
  anon_send_exit(client_actor, exit_reason::user_shutdown);
  await_all_actors_done();
  shutdown();
}
