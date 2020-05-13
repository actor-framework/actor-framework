// This example program represents a minimal terminal chat program based on
// group communication.
//
// Setup for a minimal chat between "alice" and "bob":
// - ./build/bin/group_chat -s -p 4242
// - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n alice
// - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n bob

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "caf/string_algorithms.hpp"

CAF_BEGIN_TYPE_ID_BLOCK(group_chat, first_custom_type_id)

  CAF_ADD_ATOM(group_chat, custom, broadcast_atom, "broadcast")

CAF_END_TYPE_ID_BLOCK(group_chat)

using namespace caf;
using namespace custom;

struct line {
  std::string str;
};

std::istream& operator>>(std::istream& is, line& l) {
  std::getline(is, l.str);
  return is;
}

behavior client(event_based_actor* self, const std::string& name) {
  return {
    [=](broadcast_atom, const std::string& message) {
      for (auto& dest : self->joined_groups()) {
        self->send(dest, name + ": " + message);
      }
    },
    [=](join_atom, const group& what) {
      for (const auto& g : self->joined_groups()) {
        std::cout << "*** leave " << to_string(g) << std::endl;
        self->send(g, name + " has left the chatroom");
        self->leave(g);
      }
      std::cout << "*** join " << to_string(what) << std::endl;
      self->join(what);
      self->send(what, name + " has entered the chatroom");
    },
    [=](const std::string& txt) {
      // don't print own messages
      if (self->current_sender() != self)
        std::cout << txt << std::endl;
    },
    [=](const group_down_msg& g) {
      std::cout << "*** chatroom offline: " << to_string(g.source) << std::endl;
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
              << to_string(res.error()) << std::endl;
    return;
  }
  std::cout << "*** listening at port " << *res << std::endl
            << "*** press [enter] to quit" << std::endl;
  std::string dummy;
  std::getline(std::cin, dummy);
  std::cout << "... cya" << std::endl;
}

void run_client(actor_system& system, const config& cfg) {
  auto name = cfg.name;
  while (name.empty()) {
    std::cout << "please enter your name: " << std::flush;
    if (!std::getline(std::cin, name)) {
      std::cerr << "*** no name given... terminating" << std::endl;
      return;
    }
  }
  std::cout << "*** starting client, type '/help' for a list of commands\n";
  auto client_actor = system.spawn(client, name);
  for (auto& uri : cfg.group_uris) {
    auto tmp = system.groups().get(uri);
    if (tmp)
      anon_send(client_actor, join_atom_v, std::move(*tmp));
    else
      std::cerr << R"(*** failed to parse ")" << uri << R"(" as group URI: )"
                << to_string(tmp.error()) << std::endl;
  }
  std::istream_iterator<line> eof;
  std::vector<std::string> words;
  for (std::istream_iterator<line> i{std::cin}; i != eof; ++i) {
    auto send_input = [&] {
      if (!i->str.empty())
        anon_send(client_actor, broadcast_atom_v, i->str);
    };
    words.clear();
    split(words, i->str, is_any_of(" "));
    message_handler f{
      [&](const std::string& cmd, const std::string& mod,
          const std::string& id) {
        if (cmd == "/join") {
          auto grp = system.groups().get(mod, id);
          if (grp)
            anon_send(client_actor, join_atom_v, *grp);
        } else {
          send_input();
        }
      },
      [&](const std::string& cmd) {
        if (cmd == "/quit") {
          std::cin.setstate(std::ios_base::eofbit);
        } else if (cmd[0] == '/') {
          std::cout << "*** available commands:\n"
                       "  /join <module> <group> join a new chat channel\n"
                       "  /quit          quit the program\n"
                       "  /help          print this text\n";
        } else {
          send_input();
        }
      },
    };
    auto msg = message_builder(words.begin(), words.end()).move_to_message();
    auto res = f(msg);
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

CAF_MAIN(id_block::group_chat, io::middleman)
