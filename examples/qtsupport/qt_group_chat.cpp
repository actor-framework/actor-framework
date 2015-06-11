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

CAF_PUSH_WARNINGS
#include <QMainWindow>
#include <QApplication>
#include "ui_chatwindow.h" // auto generated from chatwindow.ui
CAF_POP_WARNINGS

using namespace std;
using namespace caf;

int main(int argc, char** argv) {
  string name;
  string group_id;
  auto res = message_builder(argv + 1, argv + argc).extract_opts({
    {"name,n", "set chat name", name},
    {"group,g", "join chat group", group_id}
  });
  if (! res.error.empty()) {
    cerr << res.error << endl;
    return 1;
  }
  if (! res.remainder.empty()) {
    std::cerr << res.helptext << std::endl;
    return 1;
  }
  if (res.opts.count("help") > 0) {
    cout << res.helptext << endl;
    return 0;
  }
  group gptr;
  // evaluate group parameter
  if (! group_id.empty()) {
    auto p = group_id.find(':');
    if (p == std::string::npos) {
      cerr << "*** error parsing argument " << group_id
           << ", expected format: <module_name>:<group_id>";
    } else {
      try {
        gptr = group::get(group_id.substr(0, p), group_id.substr(p + 1));
      } catch (exception& e) {
        cerr << "*** exception: group::get(\"" << group_id.substr(0, p)
             << "\", \"" << group_id.substr(p + 1) << "\") failed; "
             << to_verbose_string(e) << endl;
      }
    }
  }
  QApplication app(argc, argv);
  app.setQuitOnLastWindowClosed(true);
  QMainWindow mw;
  Ui::ChatWindow helper;
  helper.setupUi(&mw);
  auto client = helper.chatwidget->as_actor();
  if (! name.empty()) {
    send_as(client, client, atom("setName"), move(name));
  }
  if (gptr) {
    send_as(client, client, atom("join"), gptr);
  }
  mw.show();
  auto app_res = app.exec();
  await_all_actors_done();
  shutdown();
  return app_res;
}
