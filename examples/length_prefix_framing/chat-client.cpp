// Simple chat server with a binary protocol.

#include "caf/net/lp/with_v2.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/ssl/context.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/async/blocking_producer.hpp"
#include "caf/byte_span.hpp"
#include "caf/caf_main.hpp"
#include "caf/chunk.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/flow/string.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/uri.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <span>
#include <string>
#include <utility>

using namespace std::literals;

namespace lp = caf::net::lp;
namespace ssl = caf::net::ssl;

// -- constants ----------------------------------------------------------------

static constexpr std::string_view default_uri = "lpf://localhost:7788";

static constexpr caf::net::lp::size_field_type default_size
  = caf::net::lp::size_field_type::u4;

static constexpr std::string_view default_name = "";

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<caf::uri>("uri,u", "URI of the server, e.g., lpf://localhost:7788")
      .add<caf::net::lp::size_field_type>("size,s",
                                          "length prefix size of the server")
      .add<std::string>("name,n", "set name");
    opt_group{custom_options_, "tls"} //
      .add<std::string>("ca-file", "CA file for trusted servers");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    caf::put_missing(result, "uri", *caf::make_uri(default_uri));
    caf::put_missing(result, "name", default_name);
    caf::put_missing(result, "size", default_size);
    return result;
  }
};

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  // Read the configuration.
  auto uri = caf::get_or<caf::uri>(cfg, "uri", *caf::make_uri(default_uri));
  auto size = caf::get_or(cfg, "size", default_size);
  auto name = caf::get_or(cfg, "name", default_name);
  auto ca_file = caf::get_as<std::string>(cfg, "tls.ca-file");
  if (name.empty()) {
    sys.println("*** mandatory parameter 'name' missing or empty");
    return EXIT_FAILURE;
  }
  // Set up a blocking producer for reading lines from stdin.
  auto [line_producer, line_pull]
    = caf::async::make_blocking_producer<caf::chunk>();
  // Start an asynchronous connection to the server. This does not block the
  // calling thread while establishing the connection. The URI scheme selects
  // the transport: `lpf` for plain TCP and `lpfs` for TLS.
  auto [pull, push]
    = lp::with_v2(sys)
        .async()
        .connect(std::move(uri))
        // The context factory is only invoked for `lpfs` scheme URIs.
        .context([ca_file] {
          return ssl::context::enable()
            .and_then(ssl::emplace_client(ssl::tls::v1_2))
            .and_then(ssl::load_verify_file_if(ca_file));
        })
        // Set the size field type.
        .size_field(size)
        // If we don't succeed at first, try up to 10 times with 1s delay.
        .retry_delay(1s)
        .max_retry_count(9)
        .start();
  // Spin up a worker that prints received inputs and forwards stdin to the
  // server.
  sys.spawn([lpull = std::move(line_pull), pull,
             push](caf::event_based_actor* self) mutable {
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
      .for_each([self](const caf::chunk& frame) {
        // Interpret the bytes as ASCII characters.
        auto str = caf::to_string_view(frame.bytes());
        if (std::ranges::all_of(str, ::isprint)) {
          self->println("{}", str);
        } else {
          self->println("<non-ascii-data of size {}>", frame.bytes().size());
        }
      });
    // Read what the users types and send it to the server.
    lpull.observe_on(self)
      .do_finally([self] { self->quit(); }) //
      .subscribe(push);
  });
  // Send each line to the server and stop if stdin closes or on an empty line.
  auto line = std::string{};
  auto prefix = name + ": ";
  while (std::getline(std::cin, line)) {
    line.insert(line.begin(), prefix.begin(), prefix.end());
    line_producer.push(caf::chunk{std::as_bytes(std::span{line})});
    line.clear();
  }
  sys.println("*** shutting down");
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
