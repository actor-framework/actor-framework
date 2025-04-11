// Simple chat server with a binary protocol.

#include "caf/net/lp/with.hpp"
#include "caf/net/middleman.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/blocking_producer.hpp"
#include "caf/caf_main.hpp"
#include "caf/chunk.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/string.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/span.hpp"
#include "caf/uuid.hpp"

#include <cassert>
#include <iostream>
#include <utility>

using namespace std::literals;

namespace lp = caf::net::lp;
namespace ssl = caf::net::ssl;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 7788;

static constexpr std::string_view default_host = "localhost";

static constexpr std::string_view default_name = "";

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port of the server")
      .add<std::string>("host,H", "host of the server")
      .add<std::string>("name,n", "set name");
    opt_group{custom_options_, "tls"} //
      .add<bool>("enable", "enables encryption via TLS")
      .add<std::string>("ca-file", "CA file for trusted servers");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    caf::put_missing(result, "port", default_port);
    caf::put_missing(result, "host", default_host);
    caf::put_missing(result, "name", default_name);
    return result;
  }
};

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto host = caf::get_or(cfg, "host", default_host);
  auto name = caf::get_or(cfg, "name", default_name);
  auto use_ssl = caf::get_or(cfg, "tls.enable", false);
  auto ca_file = caf::get_as<std::string>(cfg, "tls.ca-file");
  if (name.empty()) {
    sys.println("*** mandatory parameter 'name' missing or empty");
    return EXIT_FAILURE;
  }
  // Connect to the server.
  auto [line_producer, line_pull]
    = caf::async::make_blocking_producer<caf::chunk>();
  auto conn
    = caf::net::lp::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(use_ssl)
                   .and_then(ssl::emplace_client(ssl::tls::v1_2))
                   .and_then(ssl::load_verify_file_if(ca_file)))
        // Connect to "$host:$port".
        .connect(host, port)
        // If we don't succeed at first, try up to 10 times with 1s delay.
        .retry_delay(1s)
        .max_retry_count(9)
        // After connecting, spin up a worker that prints received inputs.
        .start([&sys, lpull = line_pull](auto pull, auto push) {
          sys.spawn([lpull, pull, push](caf::event_based_actor* self) {
            // Read from the server and print each line.
            pull
              .observe_on(self) //
              .do_on_error([self](const caf::error& err) {
                self->println("*** connection error: {}", err);
              })
              .do_finally([self] {
                self->println("*** lost connection to server -> quit");
                self->println("*** use CTRL+D or CTRL+C to terminate");
                self->quit();
              })
              .for_each([self](const lp::frame& frame) {
                // Interpret the bytes as ASCII characters.
                auto bytes = frame.bytes();
                auto str = std::string_view{
                  reinterpret_cast<const char*>(bytes.data()), bytes.size()};
                if (std::all_of(str.begin(), str.end(), ::isprint)) {
                  self->println("{}", str);
                } else {
                  self->println("<non-ascii-data of size {}>", bytes.size());
                }
              });
            // Read what the users types and send it to the server.
            lpull.observe_on(self)
              .do_finally([self] { self->quit(); })
              .subscribe(push);
          });
        });
  // Report any error to the user.
  if (!conn) {
    sys.println("*** unable to connect to {} on port {}: {}", host, port,
                conn.error());
    return EXIT_FAILURE;
  }
  // Send each line to the server and stop if stdin closes or on an empty line.
  auto line = std::string{};
  auto prefix = name + ": ";
  while (std::getline(std::cin, line)) {
    line.insert(line.begin(), prefix.begin(), prefix.end());
    line_producer.push(caf::chunk{caf::as_bytes(caf::make_span(line))});
    line.clear();
  }
  sys.println("*** shutting down");
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
