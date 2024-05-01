// This example program represents a minimal GUI chat program based on group
// communication. This chat program is compatible to the terminal version in
// length_prefix_framing/chat-server.cpp.
//
// Setup for a minimal chat between "alice" and "bob":
// - chat-server -p 4242
// - qt_group_chat -H localhost -p 4242 -n alice
// - qt_group_chat -H localhost -p 4242 -n bob

#include "caf/net/lp/with.hpp"
#include "caf/net/middleman.hpp"

#include "caf/all.hpp"

#include <time.h>

#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <vector>

CAF_PUSH_WARNINGS
#include "ui_chatwindow.h" // auto generated from chatwindow.ui

#include <QApplication>
#include <QMainWindow>
CAF_POP_WARNINGS

#include "chatwidget.hpp"

using namespace caf;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 7788;

static constexpr std::string_view default_host = "localhost";

// -- configuration setup ------------------------------------------------------

class config : public actor_system_config {
public:
  config() {
    opt_group{custom_options_, "global"}
      .add<uint16_t>("port,p", "port of the server")
      .add<std::string>("host,H", "host of the server")
      .add<std::string>("name,n", "set name");
  }
};

// -- main ---------------------------------------------------------------------

int caf_main(actor_system& sys, const config& cfg) {
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto host = caf::get_or(cfg, "host", default_host);
  auto name = caf::get_or(cfg, "name", "");
  if (name.empty()) {
    sys.println("*** mandatory parameter 'name' missing or empty");
    return EXIT_FAILURE;
  }
  // Spin up Qt.
  auto [argc, argv] = cfg.c_args_remainder();
  QApplication app{argc, argv};
  app.setQuitOnLastWindowClosed(true);
  QMainWindow mw;
  Ui::ChatWindow helper;
  helper.setupUi(&mw);
  // Connect to the server.
  auto conn = caf::net::lp::with(sys)
                .connect(host, port)
                .start([&](auto pull, auto push) {
                  sys.println("*** connected to {}:{}", host, port);
                  helper.chatwidget->init(sys, name, std::move(pull),
                                          std::move(push));
                });
  if (!conn) {
    sys.println("*** unable to connect to {}:{}: {}", host, port, conn.error());
    mw.close();
    return app.exec();
  }
  // Setup and run.
  mw.show();
  auto result = app.exec();
  conn->dispose();
  return result;
}

CAF_MAIN(caf::id_block::qtsupport, caf::net::middleman)
