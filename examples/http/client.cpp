// Simple HTTP client that prints the response

#include "caf/net/http/client.hpp"

#include "caf/net/http/method.hpp"
#include "caf/net/http/status.hpp"
#include "caf/net/http/with_v2.hpp"
#include "caf/net/middleman.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/byte_span.hpp"
#include "caf/caf_main.hpp"

#include <cassert>
#include <csignal>
#include <string>
#include <tuple>
#include <utility>

namespace http = caf::net::http;
namespace ssl = caf::net::ssl;

using namespace caf;
using namespace std::literals;

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

int caf_main(caf::actor_system& sys, const config& cfg) {
  // Get URI from config (positional argument).
  auto remainder = cfg.remainder();
  if (remainder.size() != 1) {
    sys.println("*** expected mandatory positional argument: URL");
    return EXIT_FAILURE;
  }
  uri resource;
  if (auto err = parse(remainder[0], resource); err.valid()) {
    sys.println("*** failed to parse URI: {} ", err);
    return EXIT_FAILURE;
  }
  auto ca_file = caf::get_as<std::string>(cfg, "tls.ca-file");
  auto method = caf::get_or(cfg, "method", default_method);
  auto payload = caf::get_or(cfg, "payload", ""sv);
  auto maybe_response = // Block this thread until the request completes.
    http::with_v2(sys)
      .sync()
      .connect(resource)
      // Lazy load TLS when connecting to HTTPS endpoints.
      .context([ca_file, resource]() {
        return ssl::context::enable()
          .and_then(ssl::emplace_client(ssl::tls::v1_2))
          .and_then(ssl::load_verify_file_if(ca_file))
          .and_then(ssl::use_sni_hostname(resource));
      })
      // If we don't succeed at first, try up to 5 times with 1s delay.
      .retry_delay(1s)
      .max_retry_count(5)
      // Wait up to 250ms for establishing a connection.
      .connection_timeout(250ms)
      .add_header_field("User-Agent", "CAF Example")
      // Send a request using the configured HTTP method and payload.
      .request(method, payload);
  // If the request failed, we simply print the error and return.
  if (!maybe_response) {
    sys.println("*** HTTP request failed: {}", maybe_response.error());
    return EXIT_FAILURE;
  }
  // Otherwise, we print the header fields and the received payload.
  const auto& response = *maybe_response;
  sys.println("Server responded with HTTP {}: {}",
              static_cast<uint16_t>(response.code()), phrase(response.code()));
  sys.println("Header fields:");
  for (const auto& [key, value] : response.header_fields()) {
    sys.println("- {}: {}", key, value);
  }
  if (auto body = response.body(); !body.empty()) {
    if (is_valid_utf8(body)) {
      sys.println("Payload (UTF-8): {}", to_string_view(body));
    } else {
      sys.println("Payload (binary): {}", to_hex_str(body));
    }
  }
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
