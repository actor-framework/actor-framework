// HTTP server example that accepts multipart/form-data uploads via POST /
// and parses request parts with caf::net::http::multipart_reader.
//
// Usage:
//   multipart-reader [--port=<port>] [--max-connections=<num>]
//                    [--max-request-size=<bytes>]
//                    [--tls.key-file=<path> --tls.cert-file=<path>]
//
// For each received part, this example prints all part header fields and the
// part content to stdout, then responds with HTTP 200. Requests with a
// non-multipart Content-Type or malformed multipart payload are rejected with
// HTTP 400.

#include "caf/net/http/multipart_reader.hpp"
#include "caf/net/http/with.hpp"
#include "caf/net/middleman.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/byte_span.hpp"
#include "caf/caf_main.hpp"
#include "caf/defaults.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <string>
#include <thread>

using namespace std::literals;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 8080;

// -- configuration ------------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<size_t>("max-connections,m", "limit for concurrent clients")
      .add<size_t>("max-request-size,r", "limit for single request size");
    opt_group{custom_options_, "tls"} //
      .add<std::string>("key-file,k", "path to the private key file")
      .add<std::string>("cert-file,c", "path to the certificate file");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    caf::put_missing(result, "port", default_port);
    caf::put_missing(result, "max-connections", size_t{128});
    caf::put_missing(result, "max-request-size",
                     caf::defaults::net::http_max_request_size);
    return result;
  }
};

// -- main ---------------------------------------------------------------------

namespace {

std::atomic<bool> shutdown_flag;

} // namespace

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace ssl = caf::net::ssl;
  namespace http = caf::net::http;
  // Do a regular shutdown for CTRL+C and SIGTERM.
  auto set_shutdown_flag = [](int) { shutdown_flag = true; };
  signal(SIGTERM, set_shutdown_flag);
  signal(SIGINT, set_shutdown_flag);
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto pem = ssl::format::pem;
  auto key_file = caf::get_as<std::string>(cfg, "tls.key-file");
  auto cert_file = caf::get_as<std::string>(cfg, "tls.cert-file");
  if (key_file.has_value() != cert_file.has_value()) {
    sys.println("*** inconsistent TLS config: declare neither file or both");
    return EXIT_FAILURE;
  }
  // Open up a TCP port for incoming connections and start the server.
  auto server
    = http::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(key_file && cert_file)
                   .and_then(ssl::emplace_server(ssl::tls::v1_2))
                   .and_then(ssl::use_private_key_file(key_file, pem))
                   .and_then(ssl::use_certificate_file(cert_file, pem)))
        .accept(port)
        .max_connections(caf::get_as<size_t>(cfg, "max-connections"))
        .max_request_size(caf::get_as<size_t>(cfg, "max-request-size"))
        .route(
          "/", http::method::post,
          [&sys](http::responder& res) {
            auto value = res.payload();
            auto& header = res.header();
            if (!header.has_token("Content-Type", "multipart/form-data")) {
              sys.println("*** expected multipart/form-data content. Found: {}",
                          header.field("Content-Type"));
              res.respond(http::status::bad_request);
              return;
            }
            auto reader = caf::net::http::multipart_reader{header, value};
            if (!reader.for_each([&sys](http::header&& part_header,
                                        caf::const_byte_span part_content) {
                  sys.println("Number of fields: {}", part_header.num_fields());
                  part_header.for_each_field(
                    [&sys](std::string_view name, std::string_view value) {
                      sys.println("{}: {}", name, value);
                    });
                  auto val = caf::to_string_view(part_content);
                  sys.println("Content: {}", val);
                })) {
              sys.println("*** failed to parse multipart data");
              res.respond(http::status::bad_request);
              return;
            }
            res.respond(http::status::ok);
          })
        // Launch the server.
        .start();
  // Report any error to the user.
  if (!server) {
    sys.println("*** unable to run at port {}: {}", port, server.error());
    return EXIT_FAILURE;
  }
  // Wait for CTRL+C or SIGTERM.
  while (!shutdown_flag) {
    std::this_thread::sleep_for(250ms);
  }
  sys.println("*** shutting down");
  server->dispose();
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
