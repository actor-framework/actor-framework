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
#include <iostream>
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
  if (cfg.remainder.size() != 1) {
    std::cerr << "*** expected mandatory positional argument: URL" << std::endl;
    return EXIT_FAILURE;
  }
  uri resource;
  if (auto err = parse(cfg.remainder[0], resource)) {
    std::cerr << "*** failed to parse URI: " << to_string(err) << std::endl;
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
        // Timeout the wait for connection after 50ms.
        .connection_timeout(50ms)
        // After connecting, send a get request.
        .add_header_field("User-Agent", "CAF-Client")
        .request(method, payload);
  if (!result) {
    std::cerr << "*** Failed to initiate connection: "
              << to_string(result.error()) << std::endl;
    return EXIT_FAILURE;
  }
  sys.spawn([res = result->first](event_based_actor* self) {
    res.bind_to(self).then(
      [](const http::response& r) {
        std::cout << "Server responded with HTTP "
                  << detail::to_underlying(r.code()) << ": " << phrase(r.code())
                  << std::endl;
        std::cout << "Header fields:" << std::endl;
        for (const auto& [key, value] : r.header_fields())
          std::cout << "- " << key << ": " << value << std::endl;
        if (r.body().empty())
          return;
        std::cout << "Payload:" << std::endl;
        if (is_valid_utf8(r.body())) {
          std::cout << to_string_view(r.body()) << std::endl;
        } else {
          auto split_at = [](const_byte_span bytes, size_t at) {
            if (bytes.size() > at)
              return std::pair{bytes.subspan(0, at), bytes.subspan(at)};
            return std::pair{bytes, const_byte_span{}};
          };
          // Print 8 bytes per row in hex.
          auto bytes = r.body();
          const_byte_span row;
          while (!bytes.empty()) {
            std::tie(row, bytes) = split_at(bytes, 8);
            for (auto byte : row)
              printf("%02x", std::to_integer<int>(byte));
            std::cout << std::endl;
          }
        }
      },
      [](const error& err) {
        std::cerr << "*** HTTP request failed: " << to_string(err) << std::endl;
      });
  });
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
