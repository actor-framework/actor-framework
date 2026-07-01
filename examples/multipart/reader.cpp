// Simple HTTP server that receives multipart/form-data uploads and
// prints parsed part headers and content to stdout.

#include "caf/net/http/multipart_reader.hpp"
#include "caf/net/http/with.hpp"
#include "caf/net/middleman.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/defaults.hpp"

#include <chrono>
#include <csignal>
#include <string>
#include <thread>

using namespace std::literals;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 8080;

static constexpr size_t default_max_connections = 128;

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
    caf::put_missing(result, "max-connections", default_max_connections);
    caf::put_missing(result, "max-request-size",
                     caf::defaults::net::http_max_request_size);
    return result;
  }
};

// -- main ---------------------------------------------------------------------

namespace {

volatile std::sig_atomic_t shutdown_flag = 0;

void set_shutdown_flag(int) {
  shutdown_flag = 1;
}

} // namespace

int caf_main(caf::actor_system& sys, const config& cfg) {
  namespace ssl = caf::net::ssl;
  namespace http = caf::net::http;
  // Do a regular shutdown for CTRL+C and SIGTERM.
  signal(SIGTERM, set_shutdown_flag);
  signal(SIGINT, set_shutdown_flag);
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto pem = ssl::format::pem;
  auto key_file = caf::get_as<std::string>(cfg, "tls.key-file");
  auto cert_file = caf::get_as<std::string>(cfg, "tls.cert-file");
  auto max_connections = caf::get_or(cfg, "max-connections",
                                     default_max_connections);
  auto max_request_size = caf::get_or(
    cfg, "max-request-size", caf::defaults::net::http_max_request_size);
  auto has_key_file = static_cast<bool>(key_file);
  auto has_cert_file = static_cast<bool>(cert_file);
  if (has_key_file != has_cert_file) {
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
        // Bind to the user-defined port.
        .accept(port)
        // Limit how many clients may be connected at any given time.
        .max_connections(max_connections)
        // Limit the maximum request size.
        .max_request_size(max_request_size)
        .route(
          "/", http::method::post,
          [&sys](http::responder& res) {
            auto value = res.payload();
            auto& header = res.header();
            const auto content_type = header.field("Content-Type");
            if (!content_type.starts_with("multipart/form-data")) {
              sys.println("*** expected multipart/form-data content. Found: {}",
                          content_type);
              res.respond(http::status::bad_request);
              return;
            }
            auto reader = caf::net::http::multipart_reader{header, value};
            bool ok = false;
            auto parts = reader.parse(&ok);
            if (!ok) {
              sys.println("*** failed to parse multipart data");
              res.respond(http::status::bad_request);
              return;
            }
            for (const auto& part : parts) {
              sys.println("Number of fields: {}", part.header.num_fields());
              part.header.for_each_field(
                [&sys](std::string_view name, std::string_view value) {
                  sys.println("{}: {}", name, value);
                });
              auto val = caf::to_string_view(part.content);
              sys.println("Content: {}", val);
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
  while (!shutdown_flag)
    std::this_thread::sleep_for(250ms);
  sys.println("*** shutting down");
  server->dispose();
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
