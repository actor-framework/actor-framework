/******************************************************************************\
 * This example program represents a minimal terminal chat program            *
 * based on group communication.                                              *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - ./build/example/group_chat -s -p 4242                                    *
 * - ./build/example/group_chat -g remote:chatroom@localhost:4242 -n alice    *
 * - ./build/example/group_chat -g remote:chatroom@localhost:4242 -n bob      *
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

CAF_BEGIN_TYPE_ID_BLOCK(group_chat, first_custom_type_id)

  CAF_ADD_ATOM(group_chat, broadcast_atom)

CAF_END_TYPE_ID_BLOCK(group_chat)

using namespace caf;

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
      auto groups = self->joined_groups();
      for (auto&& grp : groups) {
        std::cout << "*** leave " << to_string(grp) << std::endl;
        self->send(grp, name + " has left the chatroom");
        self->leave(grp);
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
    [=](leave_atom) {
      auto groups = self->joined_groups();
      for (auto&& grp : groups) {
        std::cout << "*** leave " << to_string(grp) << std::endl;
        self->send(grp, name + " has left the chatroom");
        self->leave(grp);
      }
    },
  };
}

class config : public actor_system_config {
public:
  config() {
    opt_group{custom_options_, "global"}
      .add<std::string>("name,n", "set name")
      .add<std::string>("group,g", "join group")
      .add<bool>("server,s", "run in server mode")
      .add<uint16_t>("port,p", "set port (ignored in client mode)");
  }
};

void run_server(actor_system& sys) {
  auto port = get_or(sys.config(), "port", uint16_t{0});
  auto res = sys.middleman().publish_local_groups(port);
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

void run_client(actor_system& sys) {
  std::string name;
  if (auto config_name = get_if<std::string>(&sys.config(), "name"))
    name = *config_name;
  while (name.empty()) {
    std::cout << "please enter your name: " << std::flush;
    if (!std::getline(std::cin, name)) {
      std::cerr << "*** no name given... terminating" << std::endl;
      return;
    }
  }
  auto client_actor = sys.spawn(client, name);
  if (auto locator = get_if<std::string>(&sys.config(), "group")) {
    if (auto grp = sys.groups().get(*locator)) {
      anon_send(client_actor, join_atom_v, std::move(*grp));
    } else {
      std::cerr << R"(*** failed to parse ")" << *locator
                << R"(" as group locator: )" << to_string(grp.error())
                << std::endl;
    }
  }
  std::cout << "*** starting client, type '/help' for a list of commands\n";
  std::istream_iterator<line> eof;
  std::vector<std::string> words;
  for (std::istream_iterator<line> i{std::cin}; i != eof; ++i) {
    if (i->str.empty()) {
      // Ignore empty lines.
    } else if (i->str[0] == '/') {
      words.clear();
      split(words, i->str, is_any_of(" "));
      if (words.size() == 3 && words[0] == "/join") {
        if (auto grp = sys.groups().get(words[1], words[2]))
          anon_send(client_actor, join_atom_v, *grp);
        else
          std::cerr << "*** failed to join group: " << to_string(grp.error())
                    << std::endl;
      } else if (words.size() == 1 && words[0] == "/quit") {
        std::cin.setstate(std::ios_base::eofbit);
      } else {
        std::cout << "*** available commands:\n"
                     "  /join <module> <group> join a new chat channel\n"
                     "  /quit                  quit the program\n"
                     "  /help                  print this text\n";
      }
    } else {
      anon_send(client_actor, broadcast_atom_v, i->str);
    }
  }
  anon_send(client_actor, leave_atom_v);
  anon_send_exit(client_actor, exit_reason::user_shutdown);
}

void caf_main(actor_system& sys, const config& cfg) {
  auto f = get_or(cfg, "server", false) ? run_server : run_client;
  f(sys);
}

CAF_MAIN(id_block::group_chat, io::middleman)
