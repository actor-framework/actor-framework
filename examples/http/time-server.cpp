// Simple HTTP server that tells the time.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/net/http/with.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>
#include <string>
#include <utility>

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 8080;

static constexpr size_t default_max_connections = 128;

// -- configuration ------------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<size_t>("max-connections,m", "limit for concurrent clients");
    opt_group{custom_options_, "tls"} //
      .add<std::string>("key-file,k", "path to the private key file")
      .add<std::string>("cert-file,c", "path to the certificate file");
  }
};

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace http = caf::net::http;
  namespace ssl = caf::net::ssl;
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto pem = ssl::format::pem;
  auto key_file = caf::get_as<std::string>(cfg, "tls.key-file");
  auto cert_file = caf::get_as<std::string>(cfg, "tls.cert-file");
  auto max_connections = caf::get_or(cfg, "max-connections",
                                     default_max_connections);
  if (!key_file != !cert_file) {
    std::cerr << "*** inconsistent TLS config: declare neither file or both\n";
    return EXIT_FAILURE;
  }
  // Open up a TCP port for incoming connections and start the server.
  auto server
    = caf::net::http::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(key_file && cert_file)
                   .and_then(ssl::emplace_server(ssl::tls::v1_2))
                   .and_then(ssl::use_private_key_file(key_file, pem))
                   .and_then(ssl::use_certificate_file(cert_file, pem)))
        // Bind to the user-defined port.
        .accept(port)
        // Limit how many clients may be connected at any given time.
        .max_connections(max_connections)
        // When started, run our worker actor to handle incoming connections.
        .start([&sys](auto requests) {
          // Note: requests is an async::consumer_resource<http::request>.
          sys.spawn([requests](caf::event_based_actor* self) {
            // For each incoming HTTP request ...
            requests
              .observe_on(self) //
              .for_each([](const http::request& req) {
                // ... respond with the current time as string.
                auto str = caf::deep_to_string(caf::make_timestamp());
                req.respond(http::status::ok, "text/plain", str);
                // Note: we cannot respond more than once to a request.
              });
          });
        });
  // Report any error to the user.
  if (!server) {
    std::cerr << "*** unable to run at port " << port << ": "
              << to_string(server.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Note: the actor system will keep the application running for as long as the
  // workers are still alive.
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
