// Simple WebSocket server that sends everything it receives back to the sender.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/web_socket/with.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>
#include <utility>

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 7788;

static constexpr size_t default_max_connections = 128;

// -- configuration setup ------------------------------------------------------

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

// --(rst-main-begin)--
int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace http = caf::net::http;
  namespace ssl = caf::net::ssl;
  namespace ws = caf::net::web_socket;
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
  using trait = ws::default_trait;
  auto server
    = ws::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(key_file && cert_file)
                   .and_then(ssl::emplace_server(ssl::tls::v1_2))
                   .and_then(ssl::use_private_key_file(key_file, pem))
                   .and_then(ssl::use_certificate_file(cert_file, pem)))
        // Bind to the user-defined port.
        .accept(port)
        // Limit how many clients may be connected at any given time.
        .max_connections(max_connections)
        // Accept only requests for path "/".
        .on_request([](ws::acceptor<>& acc) {
          // The header parameter contains fields from the WebSocket handshake
          // such as the path and HTTP header fields..
          auto path = acc.header().path();
          std::cout << "*** new client request for path " << path << '\n';
          // Accept the WebSocket connection only if the path is "/".
          if (path == "/") {
            // Calling `accept` causes the server to acknowledge the client and
            // creates input and output buffers that go to the worker actor.
            acc.accept();
          } else {
            // Calling `reject` aborts the connection with HTTP status code 400
            // (Bad Request). The error gets converted to a string and sent to
            // the client to give some indication to the client why the request
            // was rejected.
            auto err = caf::make_error(caf::sec::invalid_argument,
                                       "unrecognized path, try '/'");
            acc.reject(std::move(err));
          }
          // Note: calling nothing on `acc` also rejects the connection.
        })
        // When started, run our worker actor to handle incoming connections.
        .start([&sys](trait::acceptor_resource<> events) {
          sys.spawn([events](caf::event_based_actor* self) {
            // For each buffer pair, we create a new flow ...
            self->make_observable()
              .from_resource(events) //
              .for_each([self](const trait::accept_event<>& ev) {
                // ... that simply pushes data back to the sender.
                auto [pull, push] = ev.data();
                pull.observe_on(self)
                  .do_on_error([](const caf::error& what) { //
                    std::cout << "*** connection closed: " << to_string(what)
                              << "\n";
                  })
                  .do_on_complete([] { //
                    std::cout << "*** connection closed\n";
                  })
                  .do_on_next([](const ws::frame& x) {
                    if (x.is_binary()) {
                      std::cout
                        << "*** received a binary WebSocket frame of size "
                        << x.size() << '\n';
                    } else {
                      std::cout
                        << "*** received a text WebSocket frame of size "
                        << x.size() << '\n';
                    }
                  })
                  .subscribe(push);
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
  // worker from .start() is still alive.
  return EXIT_SUCCESS;
}
// --(rst-main-end)--

CAF_MAIN(caf::net::middleman)
