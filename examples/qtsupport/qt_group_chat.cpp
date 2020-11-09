/******************************************************************************\
 * This example program represents a minimal GUI chat program                 *
 * based on group communication. This chat program is compatible to the       *
 * terminal version in remote_actors/group_chat.cpp.                          *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - ./build/bin/group_server -p 4242                                         *
 * - ./build/bin/qt_group_chat -g remote:chatroom@localhost:4242 -n alice     *
 * - ./build/bin/qt_group_chat -g remote:chatroom@localhost:4242 -n bob       *
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
  std::string name;
  std::string group_id;

  config(int argc, char** argv) {
    opt_group{custom_options_, "global"}
    .add(name, "name,n", "set name")
    .add(group_id, "group,g", "join group (format: <module>:<id>");
    parse(argc, argv);
    load<io::middleman>();
    io::middleman::init_global_meta_objects();
  }
};

int main(int argc, char** argv) {
  config cfg{argc, argv};
  actor_system system{cfg};
  auto name = cfg.name;
  group grp;
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
        cerr << "*** unable to get group " << group_uri << " from module "
             << module << ": " << to_string(g.error()) << endl;
        return -1;
      }
      grp = std::move(*g);
    }
  }
  QApplication app{argc, argv};
  app.setQuitOnLastWindowClosed(true);
  QMainWindow mw;
  Ui::ChatWindow helper;
  helper.setupUi(&mw);
  helper.chatwidget->init(system);
  auto client = helper.chatwidget->as_actor();
  if (! name.empty())
    send_as(client, client, set_name_atom_v, move(name));
  if (grp)
    send_as(client, client, join_atom_v, std::move(grp));
  mw.show();
  return app.exec();
}
