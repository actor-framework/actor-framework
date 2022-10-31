/******************************************************************************\
 * This example program represents a minimal GUI chat program                 *
 * based on group communication. This chat program is compatible to the       *
 * terminal version in length_prefix_framing/chat-server.cpp.                 *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - chat-server -p 4242                                                      *
 * - qt_group_chat -H localhost -p 4242 -n alice                              *
 * - qt_group_chat -H localhost -p 4242 -n bob                                *
\******************************************************************************/

#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <time.h>
#include <vector>

#include "caf/all.hpp"
#include "caf/net/length_prefix_framing.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

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
  // Connect to the server.
  auto port = caf::get_or(cfg, "port", default_port);
  auto host = caf::get_or(cfg, "host", default_host);
  auto name = caf::get_or(cfg, "name", "");
  if (name.empty()) {
    std::cerr << "*** mandatory parameter 'name' missing or empty\n";
    return EXIT_FAILURE;
  }
  auto fd = caf::net::make_connected_tcp_stream_socket(host, port);
  if (!fd) {
    std::cerr << "*** unable to connect to " << host << ":" << port << ": "
              << to_string(fd.error()) << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "*** connected to " << host << ":" << port << ": " << '\n';
  // Create our buffers that connect the worker to the socket.
  using bin_frame = caf::net::binary::frame;
  using caf::async::make_spsc_buffer_resource;
  auto [lpf_pull, app_push] = make_spsc_buffer_resource<bin_frame>();
  auto [app_pull, lpf_push] = make_spsc_buffer_resource<bin_frame>();
  // Spin up the network backend.
  using trait = caf::net::binary::default_trait;
  using lpf = caf::net::length_prefix_framing::bind<trait>;
  auto conn = lpf::run(sys, *fd, std::move(lpf_pull), std::move(lpf_push));
  // Spin up Qt.
  auto [argc, argv] = cfg.c_args_remainder();
  QApplication app{argc, argv};
  app.setQuitOnLastWindowClosed(true);
  QMainWindow mw;
  Ui::ChatWindow helper;
  helper.setupUi(&mw);
  helper.chatwidget->init(sys, name, std::move(app_pull), std::move(app_push));
  // Setup and run.
  auto client = helper.chatwidget->as_actor();
  mw.show();
  auto result = app.exec();
  conn.dispose();
  return result;
}

CAF_MAIN(caf::id_block::qtsupport, caf::net::middleman)
