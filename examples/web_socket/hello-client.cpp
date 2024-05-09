// Simple WebSocket server that sends everything it receives back to the sender.

#include "caf/net/middleman.hpp"
#include "caf/net/web_socket/frame.hpp"
#include "caf/net/web_socket/with.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>
#include <utility>

using namespace std::literals;

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
      .add<caf::uri>("server,s", "URI for connecting to the server")
      .add<std::string>("protocols,p", "sets the Sec-WebSocket-Protocol field")
      .add<size_t>("max,m", "maximum number of message to receive");
  }
};

// --(rst-main-begin)--
int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace ws = caf::net::web_socket;
  // Sanity checking.
  auto server = caf::get_as<caf::uri>(cfg, "server");
  if (!server) {
    sys.println("*** mandatory argument 'server' missing or invalid");
    return EXIT_FAILURE;
  }
  // Ask the user for the hello message.
  std::string hello;
  std::cout << "Please enter a hello message for the server: " << std::flush;
  std::getline(std::cin, hello);
  // Try to establish a connection to the server and send the hello message.
  auto conn
    = ws::with(sys)
        // Connect to the given URI.
        .connect(server)
        // If we don't succeed at first, try up to 10 times with 1s delay.
        .retry_delay(1s)
        .max_retry_count(9)
        // On success, spin up a worker to manage the connection.
        .start([&sys, hello](auto pull, auto push) {
          sys.spawn([hello, pull, push](caf::event_based_actor* self) {
            // Open the pull handle.
            pull
              .observe_on(self)
              // Print errors to stderr.
              .do_on_error([self](const caf::error& what) {
                self->println("*** error while reading from the WebSocket: {}",
                              what);
              })
              // Restrict how many messages we receive if the user configured
              // a limit.
              .compose([self](auto in) {
                if (auto limit = caf::get_as<size_t>(self->config(), "max")) {
                  return std::move(in).take(*limit).as_observable();
                } else {
                  return std::move(in).as_observable();
                }
              })
              // Print a bye-bye message if the server closes the connection.
              .do_on_complete([self] { //
                self->println("Server has closed the connection");
              })
              // Print everything from the server to stdout.
              .for_each([self](const ws::frame& msg) {
                if (msg.is_text()) {
                  self->println("Server: {}", msg.as_text());
                } else if (msg.is_binary()) {
                  self->println("Server: [binary message of size {}]",
                                msg.as_binary().size());
                }
              });
            // Send our hello message and wait until the server closes the
            // socket. We keep the connection alive by never closing the write
            // channel.
            self->make_observable()
              .just(ws::frame{hello})
              .concat(self->make_observable().never<ws::frame>())
              .subscribe(push);
          });
        });
  if (!conn) {
    sys.println("*** unable to connect to {}: {}", server->str(), conn.error());
    return EXIT_FAILURE;
  }
  // Note: the actor system will keep the application running for as long as the
  // workers are still alive.
  return EXIT_SUCCESS;
}
// --(rst-main-end)--

CAF_MAIN(caf::net::middleman)
