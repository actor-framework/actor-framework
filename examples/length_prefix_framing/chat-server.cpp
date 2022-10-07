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
#include "caf/uuid.hpp"

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

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections");
  }
};

// -- multiplexing logic -------------------------------------------------------

void worker_impl(caf::event_based_actor* self,
                 caf::async::consumer_resource<lpf::accept_event_t> events) {
  // Allows us to push new flows into the central merge point.
  caf::flow::item_publisher<caf::flow::observable<message_t>> msg_pub{self};
  // Our central merge point combines all inputs into a single, shared flow.
  auto messages = msg_pub.as_observable().merge().share();
  // Have one subscription for debug output. This also makes sure that the
  // shared observable stays subscribed to the merger.
  messages.for_each([](const message_t& msg) {
    const auto& [conn, frame] = msg;
    std::cout << "*** got message of size " << frame.size() << " from "
              << to_string(conn) << '\n';
  });
  // Connect the flows for each incoming connection.
  events
    .observe_on(self) //
    .for_each(
      [self, messages, pub = std::move(msg_pub)] //
      (const lpf::accept_event_t& event) mutable {
        // Each connection gets a unique ID.
        auto conn = caf::uuid::random();
        std::cout << "*** accepted new connection " << to_string(conn) << '\n';
        auto& [pull, push] = event.data();
        // Subscribe the `push` end to the central merge point.
        messages
          .filter([conn](const message_t& msg) {
            // Drop all messages received by this conn.
            return msg.first != conn;
          })
          .map([](const message_t& msg) {
            // Remove the server-internal UUID.
            return msg.second;
          })
          .subscribe(push);
        // Feed messages from the `pull` end into the central merge point.
        auto inputs = pull.observe_on(self)
                        .on_error_complete() // Cary on if a connection breaks.
                        .do_on_complete([conn] {
                          std::cout << "*** lost connection " << to_string(conn)
                                    << '\n';
                        })
                        .map([conn](const bin_frame& frame) {
                          return message_t{conn, frame};
                        })
                        .as_observable();
        pub.push(inputs);
      });
}

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  // Open up a TCP port for incoming connections.
  auto port = caf::get_or(cfg, "port", default_port);
  caf::net::tcp_accept_socket fd;
  if (auto maybe_fd = caf::net::make_tcp_accept_socket(port)) {
    std::cout << "*** started listening for incoming connections on port "
              << port << '\n';
    fd = std::move(*maybe_fd);
  } else {
    std::cerr << "*** unable to open port " << port << ": "
              << to_string(maybe_fd.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Create buffers to signal events from the caf.net backend to the worker.
  auto [wres, sres] = lpf::make_accept_event_resources();
  // Spin up a worker to multiplex the messages.
  auto worker = sys.spawn(worker_impl, std::move(wres));
  // Set everything in motion.
  lpf::accept(sys, fd, std::move(sres));
  // Done. However, the actor system will keep the application running for as
  // long as actors are still alive and for as long as it has open ports or
  // connections. Since we never close the accept socket, this means the server
  // is running indefinitely until the process gets killed (e.g., via CTRL+C).
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
