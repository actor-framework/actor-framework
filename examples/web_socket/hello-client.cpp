// Simple WebSocket server that sends everything it receives back to the sender.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/net/web_socket/connect.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>
#include <utility>

static constexpr uint16_t default_port = 8080;

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<std::string>("host", "TCP endpoint to connect to")
      .add<uint16_t>("port,p", "port for connecting to the host")
      .add<std::string>("endpoint", "sets the Request-URI field")
      .add<std::string>("protocols", "sets the Sec-WebSocket-Protocol field")
      .add<size_t>("max,m", "maximum number of message to receive");
  }
};

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace cn = caf::net;
  namespace ws = cn::web_socket;
  // Sanity checking.
  auto host = caf::get_or(cfg, "host", "");
  if (host.empty()) {
    std::cerr << "*** missing mandatory argument: host\n";
    return EXIT_FAILURE;
  }
  // Ask user for the hello message.
  std::string hello;
  std::cout << "Please enter a hello message for the server: " << std::flush;
  std::getline(std::cin, hello);
  // Open up the TCP connection.
  auto port = caf::get_or(cfg, "port", default_port);
  cn::tcp_stream_socket fd;
  if (auto maybe_fd = cn::make_connected_tcp_stream_socket(host, port)) {
    std::cout << "*** connected to " << host << ':' << port << '\n';
    fd = std::move(*maybe_fd);
  } else {
    std::cerr << "*** unable to connect to " << host << ':' << port << ": "
              << to_string(maybe_fd.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Spin up the WebSocket.
  ws::handshake hs;
  hs.host(host);
  hs.endpoint(caf::get_or(cfg, "endpoint", "/"));
  if (auto str = caf::get_as<std::string>(cfg, "protocols");
      str && !str->empty())
    hs.protocols(std::move(*str));
  ws::connect(sys, fd, std::move(hs), [&](const ws::connect_event_t& conn) {
    sys.spawn([conn, hello](caf::event_based_actor* self) {
      auto [pull, push] = conn.data();
      // Print everything from the server to stdout.
      pull.observe_on(self)
        .do_on_error([](const caf::error& what) {
          std::cerr << "*** error while reading from the WebSocket: "
                    << to_string(what) << '\n';
        })
        .compose([self](auto in) {
          if (auto limit = caf::get_as<size_t>(self->config(), "max")) {
            return std::move(in).take(*limit).as_observable();
          } else {
            return std::move(in).as_observable();
          }
        })
        .for_each([](const ws::frame& msg) {
          if (msg.is_text()) {
            std::cout << "Server: " << msg.as_text() << '\n';
          } else if (msg.is_binary()) {
            std::cout << "Server: [binary message of size "
                      << msg.as_binary().size() << "]\n";
          }
        });
      // Send our hello message and wait until the server closes the socket.
      self->make_observable()
        .just(ws::frame{hello})
        .concat(self->make_observable().never<ws::frame>())
        .subscribe(push);
    });
  });
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
