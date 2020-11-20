/******************************************************************************\
 * This example program represents a minimal GUI chat program                 *
 * based on group communication. This chat program is compatible to the       *
 * terminal version in remote_actors/group_chat.cpp.                          *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - group_server -p 4242                                                     *
 * - qt_group_chat -g remote:chatroom@localhost:4242 -n alice                 *
 * - qt_group_chat -g remote:chatroom@localhost:4242 -n bob                   *
\******************************************************************************/

#include <set>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <time.h>
#include <cstdlib>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

CAF_PUSH_WARNINGS
#include <QMainWindow>
#include <QApplication>
#include "ui_chatwindow.h" // auto generated from chatwindow.ui
CAF_POP_WARNINGS

#include "chatwidget.hpp"

using namespace std;
using namespace caf;

class config : public actor_system_config {
public:
  config() {
    opt_group{custom_options_, "global"}
      .add<std::string>("name,n", "set name")
      .add<std::string>("group,g", "join group");
  }
};

int main(int argc, char** argv) {
  init_global_meta_objects<id_block::qt_support>();
  core::init_global_meta_objects();
  io::middleman::init_global_meta_objects();
  config cfg{};
  auto err = cfg.parse(argc, argv);
  if (cfg.cli_helptext_printed)
    return 0;
  cfg.load<io::middleman>();
  actor_system sys{cfg};
  std::string name;
  if (auto config_name = get_if<std::string>(&cfg, "name"))
    name = *config_name;
  while (name.empty()) {
    std::cout << "please enter your name: " << std::flush;
    if (!std::getline(std::cin, name)) {
      std::cerr << "*** no name given... terminating" << std::endl;
      return EXIT_FAILURE;
    }
  }
  group grp;
  // evaluate group parameters
  if (auto locator = get_if<std::string>(&cfg, "group")) {
    if (auto grp_p = sys.groups().get(*locator)) {
      grp = std::move(*grp_p);
    } else {
      std::cerr << R"(*** failed to parse ")" << *locator
                << R"(" as group locator: )" << to_string(grp_p.error())
                << std::endl;
    }
  }
  QApplication app{argc, argv};
  app.setQuitOnLastWindowClosed(true);
  QMainWindow mw;
  Ui::ChatWindow helper;
  helper.setupUi(&mw);
  helper.chatwidget->init(sys);
  auto client = helper.chatwidget->as_actor();
  if (! name.empty())
    send_as(client, client, set_name_atom_v, move(name));
  if (grp)
    send_as(client, client, join_atom_v, std::move(grp));
  mw.show();
  return app.exec();
}
