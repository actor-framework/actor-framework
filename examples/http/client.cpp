// Simple HTTP client that prints the response

#include "caf/net/http/client.hpp"

#include "caf/net/http/with.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/detail/latch.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/scheduled_actor/flow.hpp"
#include "caf/span.hpp"
#include "caf/uuid.hpp"

#include <cassert>
#include <csignal>
#include <memory>
#include <utility>

using namespace std::literals;

namespace http = caf::net::http;
namespace ssl = caf::net::ssl;
using namespace caf;

// -- constants ----------------------------------------------------------------

constexpr auto default_method = http::method::get;

// -- configuration setup ------------------------------------------------------
struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
      .add<http::method>("method,m", "HTTP method to use")
      .add<std::string>("payload,p", "Optional payload to send");
    opt_group{custom_options_, "tls"} //
      .add<std::string>("ca-file", "CA file for trusted servers");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    caf::put_missing(result, "method", default_method);
    return result;
  }
};

namespace {

std::atomic<bool> shutdown_flag;

} // namespace

int caf_main(caf::actor_system& sys, const config& cfg) {
  signal(SIGTERM, [](int) { shutdown_flag = true; });
  // Get URI from config (positional argument).
  auto remainder = cfg.remainder();
  if (remainder.size() != 1) {
    sys.println("*** expected mandatory positional argument: URL");
    return EXIT_FAILURE;
  }
  uri resource;
  if (auto err = parse(remainder[0], resource)) {
    sys.println("*** failed to parse URI: {} ", err);
    return EXIT_FAILURE;
  }
  auto ca_file = caf::get_as<std::string>(cfg, "tls.ca-file");
  auto method = caf::get_or(cfg, "method", default_method);
  auto payload = caf::get_or(cfg, "payload", ""sv);
  auto result
    = http::with(sys)
        // Lazy load TLS when connecting to HTTPS endpoints.
        .context_factory([ca_file]() {
          return ssl::emplace_client(ssl::tls::v1_2)().and_then(
            ssl::load_verify_file_if(ca_file));
        })
        // Connect to the address of the resource.
        .connect(resource)
        // If we don't succeed at first, try up to 5 times with 1s delay.
        .retry_delay(1s)
        .max_retry_count(5)
        // Wait up to 250ms for establishing a connection.
        .connection_timeout(250ms)
        // After connecting, send a get request.
        .add_header_field("User-Agent", "CAF-Client")
        .request(method, payload);
  if (!result) {
    sys.println("*** Failed to initiate connection: {}", result.error());
    return EXIT_FAILURE;
  }
  sys.spawn([res = result->first](event_based_actor* self) {
    res.bind_to(self).then(
      [self](const http::response& r) {
        self->println("Server responded with HTTP {}: {}",
                      static_cast<uint16_t>(r.code()), phrase(r.code()));
        self->println("Header fields:");
        for (const auto& [key, value] : r.header_fields())
          self->println("- {}: {}", key, value);
        if (r.body().empty())
          return;
        if (is_valid_utf8(r.body())) {
          self->println("Payload: {}", to_string_view(r.body()));
        } else {
          auto split_at = [](const_byte_span bytes, size_t at) {
            if (bytes.size() > at)
              return std::pair{bytes.subspan(0, at), bytes.subspan(at)};
            return std::pair{bytes, const_byte_span{}};
          };
          // Print 8 bytes per row in hex.
          self->println("Payload:");
          auto bytes = r.body();
          const_byte_span row;
          while (!bytes.empty()) {
            std::tie(row, bytes) = split_at(bytes, 8);
            self->println("{}", to_hex_str(row));
          }
        }
      },
      [self](const error& err) {
        self->println("*** HTTP request failed: {}", err);
      });
  });
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
