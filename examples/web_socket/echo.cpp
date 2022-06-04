// Simple WebSocket server that sends everything it receives back to the sender.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/net/web_socket/accept.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>
#include <utility>

static constexpr uint16_t default_port = 8080;

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections");
  }
};

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace cn = caf::net;
  namespace ws = cn::web_socket;
  // Open up a TCP port for incoming connections.
  auto port = caf::get_or(cfg, "port", default_port);
  cn::tcp_accept_socket fd;
  if (auto maybe_fd = cn::make_tcp_accept_socket({caf::ipv4_address{}, port})) {
    std::cout << "*** started listening for incoming connections on port "
              << port << '\n';
    fd = std::move(*maybe_fd);
  } else {
    std::cerr << "*** unable to open port " << port << ": "
              << to_string(maybe_fd.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Convenience type aliases.
  using event_t = ws::accept_event_t<>;
  // Create buffers to signal events from the WebSocket server to the worker.
  auto [wres, sres] = ws::make_accept_event_resources();
  // Spin up a worker to handle the events.
  auto worker = sys.spawn([worker_res = wres](caf::event_based_actor* self) {
    // For each buffer pair, we create a new flow ...
    self->make_observable()
      .from_resource(worker_res)
      .for_each([self](const event_t& event) {
        // ... that simply pushes data back to the sender.
        auto [pull, push] = event.data();
        self->make_observable()
          .from_resource(pull)
          .do_on_next([](const ws::frame& x) {
            if (x.is_binary()) {
              std::cout << "*** received a binary WebSocket frame of size "
                        << x.size() << '\n';
            } else {
              std::cout << "*** received a text WebSocket frame of size "
                        << x.size() << '\n';
            }
          })
          .subscribe(push);
      });
  });
  // Callback for incoming WebSocket requests.
  auto on_request = [](const caf::settings& hdr, auto& req) {
    // The hdr parameter is a dictionary with fields from the WebSocket
    // handshake such as the path.
    auto path = caf::get_or(hdr, "web-socket.path", "/");
    std::cout << "*** new client request for path " << path << '\n';
    // Accept the WebSocket connection only if the path is "/".
    if (path == "/") {
      // Calling `accept` causes the server to acknowledge the client and
      // creates input and output buffers that go to the worker actor.
      req.accept();
    } else {
      // Calling `reject` aborts the connection with HTTP status code 400 (Bad
      // Request). The error gets converted to a string and send to the client
      // to give some indication to the client why the request was rejected.
      auto err = caf::make_error(caf::sec::invalid_argument,
                                 "unrecognized path, try '/'");
      req.reject(std::move(err));
    }
    // Note: calling neither accept nor reject also rejects the connection.
  };
  // Set everything in motion.
  ws::accept(sys, fd, std::move(sres), on_request);
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
