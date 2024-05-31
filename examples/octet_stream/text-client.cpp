// A client for a line-based text protocol over TCP.

#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/with.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/anon_mail.hpp"
#include "caf/async/blocking_producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/caf_main.hpp"
#include "caf/flow/byte.hpp"
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
    caf::put_missing(result, "enable", false);
    return result;
  }
};

// -- main ---------------------------------------------------------------------

namespace {

std::atomic<bool> shutdown_flag;

void set_shutdown_flag(int) {
  shutdown_flag = true;
}

void worker(caf::event_based_actor* self,
            caf::async::consumer_resource<std::byte> pull,
            caf::async::consumer_resource<std::byte> input_consumer,
            caf::async::producer_resource<std::byte> push) {
  auto producer = caf::async::make_blocking_producer(push);
  self->make_observable()
    .from_resource(std::move(pull))
    .transform(caf::flow::byte::split_as_utf8_at('\n'))
    .do_on_error([self](const caf::error& err) {
      self->println("*** connection error: {}", err);
      self->quit();
    })
    .do_finally([self] {
      self->println("*** lost connection to server -> quit\n*** use "
                    "CTRL+D or CTRL+C to terminate");
      self->quit();
    })
    .for_each([self](const caf::cow_string& line) {
      self->println("reply: {}", line.str());
    });
  self->make_observable()
    .from_resource(std::move(input_consumer))
    .transform(caf::flow::byte::split_as_utf8_at('\n'))
    .do_on_error([self](const caf::error& err) {
      self->println(
        "*** input error: {}\n*** use CTRL+D or CTRL+C to terminate", err);
      self->quit();
    })
    .do_finally([self] {
      self->println("*** lost connection to input -> quit\n*** use "
                    "CTRL+D or CTRL+C to terminate");
      self->quit();
    })
    .for_each(
      [producer = std::move(producer)](const caf::cow_string& line) mutable {
        assert(producer);
        std::string message = line.str() + '\n';
        producer->push(caf::as_bytes(caf::make_span(message)));
      });
}

} // namespace

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace ssl = caf::net::ssl;
  // Do a regular shutdown for CTRL+C and SIGTERM.
  auto default_term = signal(SIGTERM, set_shutdown_flag);
  auto default_sint = signal(SIGINT, set_shutdown_flag);
  // Read the configuration.
  auto use_tls = caf::get_or(cfg, "tls.enable", false);
  auto port = caf::get_or(cfg, "port", default_port);
  auto host = caf::get_or(cfg, "host", default_host);
  auto ca_file = caf::get_as<std::string>(cfg, "tls.ca-file");
  auto input_resource = caf::async::make_spsc_buffer_resource<std::byte>();
  auto& input_consumer_resource = input_resource.first;
  auto& input_producer_resource = input_resource.second;
  auto input_producer
    = caf::async::make_blocking_producer(input_producer_resource);
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
        .start([&sys, input_consumer_resource](auto pull, auto push) {
          sys.spawn<caf::detached>(worker, std::move(pull),
                                   std::move(input_consumer_resource),
                                   std::move(push));
        });
  // Report any error to the user.
  if (!conn) {
    sys.println("*** unable to run at port {}: {}", port, conn.error());
    return EXIT_FAILURE;
  }
  // Wait for CTRL+C or SIGTERM.
  sys.println("*** Client is running, press CTRL+C to stop");
  auto line = std::string{};
  while (std::getline(std::cin, line)) {
    line = line + '\n';
    input_producer->push(caf::as_bytes(caf::make_span(line)));
    line.clear();
  }
  while (!shutdown_flag)
    std::this_thread::sleep_for(250ms);
  // Restore the default handlers.
  signal(SIGTERM, default_term);
  signal(SIGINT, default_sint);
  // Shut down the client.
  sys.println("*** shutting down");
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
