// This example program represents a minimal GUI chat program based on group
// communication. This chat program is compatible to the terminal version in
// length_prefix_framing/chat-server.cpp.
//
// Setup for a minimal chat between "alice" and "bob":
// - chat-server -p 4242
// - qt_group_chat -u lpf://localhost:4242 -n alice
// - qt_group_chat -u lpf://localhost:4242 -n bob

#include "caf/net/lp/with_v2.hpp"
#include "caf/net/middleman.hpp"

#include "caf/all.hpp"

#include <time.h>

#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

CAF_PUSH_WARNINGS
#include "ui_chatwindow.h" // auto generated from chatwindow.ui

#include <QApplication>
#include <QMainWindow>
CAF_POP_WARNINGS

#include "chatwidget.hpp"

using namespace caf;

// -- constants ----------------------------------------------------------------

static constexpr std::string_view default_uri = "lpf://localhost:7788";

// -- configuration setup ------------------------------------------------------

class config : public actor_system_config {
public:
  config() {
    opt_group{custom_options_, "global"}
      .add<caf::uri>("uri,u", "URI of the server, e.g., lpf://localhost:7788")
      .add<std::string>("name,n", "set name");
  }
};

// -- main ---------------------------------------------------------------------

int caf_main(actor_system& sys, const config& cfg) {
  // Read the configuration.
  auto uri = caf::get_or<caf::uri>(cfg, "uri", *caf::make_uri(default_uri));
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
  // Start an asynchronous connection to the server. This does not block the
  // calling thread (and thus the GUI) while establishing the connection. The
  // URI scheme selects the transport: `lpf` for plain TCP and `lpfs` for TLS.
  sys.println("*** connecting to {}", uri.str());
  auto [pull, push]
    = caf::net::lp::with_v2(sys).async().connect(std::move(uri)).start();
  helper.chatwidget->init(sys, name, std::move(pull), std::move(push));
  // Setup and run.
  mw.show();
  return app.exec();
}

CAF_MAIN(caf::id_block::qtsupport, caf::net::middleman)
