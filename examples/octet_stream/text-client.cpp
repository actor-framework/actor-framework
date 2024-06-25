// A client for a line-based text protocol over TCP.

#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/with.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/anon_mail.hpp"
#include "caf/async/blocking_producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/caf_main.hpp"
#include "caf/flow/byte.hpp"
#include "caf/flow/string.hpp"
#include "caf/json_reader.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>

using namespace std::literals;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 7788;

static constexpr std::string_view default_host = "localhost";

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<std::string>("host,h", "server host")
      .add<uint16_t>("port,p", "port to listen for incoming connections");
    opt_group{custom_options_, "tls"} //
      .add<bool>("enable,t", "enables encryption via TLS")
      .add<std::string>("ca-file", "CA file for trusted servers");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    caf::put_missing(result, "host", default_host);
    caf::put_missing(result, "port", default_port);
    return result;
  }
};

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace ssl = caf::net::ssl;
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto host = caf::get_or(cfg, "host", default_host);
  auto use_tls = caf::get_or(cfg, "tls.enable", false);
  auto ca_file = caf::get_as<std::string>(cfg, "tls.ca-file");
  // Connect to the server.
  auto [line_producer, line_pull]
    = caf::async::make_blocking_producer<caf::cow_string>();
  auto conn
    = caf::net::octet_stream::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(use_tls)
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
            // Print each line received from the server.
            pull.observe_on(self)
              .do_on_error([self](const caf::error& err) {
                self->println("*** connection error: {}", err);
                self->quit();
              })
              .do_finally([self] {
                self->println("*** lost connection to server");
                self->quit();
              })
              .transform(caf::flow::byte::split_as_utf8_at('\n'))
              .for_each([self](const caf::cow_string& line) {
                self->println("reply: {}", line.str());
              });
            // Read what the users types and send it to the server.
            lpull.observe_on(self)
              .do_finally([self] { self->quit(); })
              .transform(caf::flow::string::to_chars("\n"))
              .map([](char ch) { return static_cast<std::byte>(ch); })
              .subscribe(push);
          });
        });
  // Report any error to the user.
  if (!conn) {
    sys.println("*** unable to run at port {}: {}", port, conn.error());
    return EXIT_FAILURE;
  }
  // Send each line to the server and stop if stdin closes or on an empty line.
  sys.println("*** server is running, enter an empty line (or CTRL+D) to stop");
  auto line = std::string{};
  while (std::getline(std::cin, line)) {
    sys.println("line: {}", line);
    line_producer.push(caf::cow_string{line});
    line.clear();
  }
  sys.println("*** shutting down");
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
