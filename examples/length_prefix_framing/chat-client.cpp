// Simple chat server with a binary protocol.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/net/length_prefix_framing.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/span.hpp"
#include "caf/uuid.hpp"

#include <cassert>
#include <iostream>
#include <utility>

// -- convenience type aliases -------------------------------------------------

// Takes care converting a byte stream into a sequence of messages on the wire.
using lpf = caf::net::length_prefix_framing;

// An implicitly shared type for storing a binary frame.
using bin_frame = caf::net::binary::frame;

// Each client gets a UUID for identifying it. While processing messages, we add
// this ID to the input to tag it.
using message_t = std::pair<caf::uuid, bin_frame>;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 7788;

static constexpr std::string_view default_host = "localhost";

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port of the server")
      .add<std::string>("host,H", "host of the server")
      .add<std::string>("name,n", "set name");
  }
};

int caf_main(caf::actor_system& sys, const config& cfg) {
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
  using caf::async::make_spsc_buffer_resource;
  auto [lpf_pull, app_push] = make_spsc_buffer_resource<bin_frame>();
  auto [app_pull, lpf_push] = make_spsc_buffer_resource<bin_frame>();
  // Spin up the network backend.
  lpf::run(sys, *fd, std::move(lpf_pull), std::move(lpf_push));
  // Spin up a worker that simply prints received inputs.
  sys.spawn([pull = std::move(app_pull)](caf::event_based_actor* self) {
    pull
      .observe_on(self) //
      .do_finally([self] {
        std::cout << "*** lost connection to server -> quit\n"
                  << "*** use CTRL+D or CTRL+C to terminate\n";
        self->quit();
      })
      .for_each([](const bin_frame& frame) {
        // Interpret the bytes as ASCII characters.
        auto bytes = frame.bytes();
        auto str = std::string_view{reinterpret_cast<const char*>(bytes.data()),
                                    bytes.size()};
        if (std::all_of(str.begin(), str.end(), ::isprint)) {
          std::cout << str << '\n';
        } else {
          std::cout << "<non-ascii-data of size " << bytes.size() << ">\n";
        }
      });
  });
  // Wait for user input on std::cin and send them to the server.
  auto push_buf = app_push.try_open();
  assert(push_buf != nullptr);
  auto inputs = caf::async::blocking_producer<bin_frame>{std::move(push_buf)};
  auto line = std::string{};
  auto prefix = name + ": ";
  while (std::getline(std::cin, line)) {
    line.insert(line.begin(), prefix.begin(), prefix.end());
    inputs.push(bin_frame{caf::as_bytes(caf::make_span(line))});
    line.clear();
  }
  // Done. However, the actor system will keep the application running for as
  // long as it has open ports or connections.
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
