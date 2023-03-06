// Simple chat server with a binary protocol.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/net/lp/with.hpp"
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

// The trait for translating between bytes on the wire and flow items. The
// binary default trait operates on binary::frame items.
using trait = caf::net::binary::default_trait;

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

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  // Read the configuration.
  bool had_error = false;
  auto port = caf::get_or(cfg, "port", default_port);
  auto host = caf::get_or(cfg, "host", default_host);
  auto name = caf::get_or(cfg, "name", "");
  if (name.empty()) {
    std::cerr << "*** mandatory parameter 'name' missing or empty\n";
    return EXIT_FAILURE;
  }
  // Connect to the server.
  caf::net::lp::with(sys)
    .connect(host, port)
    .do_on_error([&](const caf::error& what) {
      std::cerr << "*** unable to connect to " << host << ":" << port << ": "
                << to_string(what) << '\n';
      had_error = true;
    })
    .start([&sys, name](auto pull, auto push) {
      // Spin up a worker that prints received inputs.
      sys.spawn([pull](caf::event_based_actor* self) {
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
            auto str = std::string_view{
              reinterpret_cast<const char*>(bytes.data()), bytes.size()};
            if (std::all_of(str.begin(), str.end(), ::isprint)) {
              std::cout << str << '\n';
            } else {
              std::cout << "<non-ascii-data of size " << bytes.size() << ">\n";
            }
          });
      });
      // Spin up a second worker that reads from std::cin and sends each line to
      // the server. Put that to its own thread since it's doing I/O.
      sys.spawn<caf::detached>([push, name] {
        auto lines = caf::async::make_blocking_producer(push);
        if (!lines)
          throw std::logic_error("failed to create blocking producer");
        auto line = std::string{};
        auto prefix = name + ": ";
        while (std::getline(std::cin, line)) {
          line.insert(line.begin(), prefix.begin(), prefix.end());
          lines->push(bin_frame{caf::as_bytes(caf::make_span(line))});
          line.clear();
        }
      });
    });
  // Note: the actor system will keep the application running for as long as the
  // workers are still alive.
  return had_error ? EXIT_FAILURE : EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
