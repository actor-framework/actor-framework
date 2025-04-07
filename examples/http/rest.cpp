// This example an HTTP server that implements a REST API by forwarding requests
// to an actor. The actor in this example is a simple key-value store. The actor
// is not aware of HTTP and the HTTP server is sending regular request messages
// to actor.

#include "caf/net/actor_shell.hpp"
#include "caf/net/http/with.hpp"
#include "caf/net/middleman.hpp"

#include "caf/actor_from_state.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/defaults.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/format_to_error.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <csignal>
#include <string>
#include <utility>

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

// -- our key-value store actor ------------------------------------------------

struct kvs_actor_state {
  caf::behavior make_behavior() {
    using caf::result;
    return {
      [this](caf::get_atom, const std::string& key) -> result<std::string> {
        if (auto i = data.find(key); i != data.end())
          return i->second;
        else
          return format_to_error(caf::sec::no_such_key, "{} not found", key);
      },
      [this](caf::put_atom, std::string key, std::string value) {
        data.insert_or_assign(std::move(key), std::move(value));
      },
      [this](caf::delete_atom, const std::string& key) { data.erase(key); },
    };
  }

  std::map<std::string, std::string> data;
};

// -- main ---------------------------------------------------------------------

namespace {

std::atomic<bool> shutdown_flag;

void set_shutdown_flag(int) {
  shutdown_flag = true;
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
  if (!key_file != !cert_file) {
    sys.println("*** inconsistent TLS config: declare neither file or both");
    return EXIT_FAILURE;
  }
  // Spin up our key-value store actor.
  auto kvs = sys.spawn(caf::actor_from_state<kvs_actor_state>);
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
        // Stop the server if our key-value store actor terminates.
        .monitor(kvs)
        // Forward incoming requests to the kvs actor.
        .route("/api/<arg>", http::method::get,
               [kvs](http::responder& res, std::string key) {
                 auto* self = res.self();
                 auto prom = std::move(res).to_promise();
                 self->mail(caf::get_atom_v, std::move(key))
                   .request(kvs, 2s)
                   .then(
                     [prom](const std::string& value) mutable {
                       prom.respond(http::status::ok, "text/plain", value);
                     },
                     [prom](const caf::error& what) mutable {
                       if (what == caf::sec::no_such_key)
                         prom.respond(http::status::not_found, "text/plain",
                                      "Key not found.");
                       else
                         prom.respond(http::status::internal_server_error,
                                      what);
                     });
               })
        .route("/api/<arg>", http::method::post,
               [kvs](http::responder& res, std::string key) {
                 auto value = res.payload();
                 if (!caf::is_valid_ascii(value)) {
                   res.respond(http::status::bad_request, "text/plain",
                               "Expected an ASCII payload.");
                   return;
                 }
                 auto* self = res.self();
                 auto prom = std::move(res).to_promise();
                 self
                   ->mail(caf::put_atom_v, std::move(key),
                          caf::to_string_view(value))
                   .request(kvs, 2s)
                   .then(
                     [prom]() mutable {
                       prom.respond(http::status::no_content);
                     },
                     [prom](const caf::error& what) mutable {
                       prom.respond(http::status::internal_server_error, what);
                     });
               })
        .route("/api/<arg>", http::method::del,
               [kvs](http::responder& res, std::string key) {
                 auto* self = res.self();
                 auto prom = std::move(res).to_promise();
                 self->mail(caf::delete_atom_v, std::move(key))
                   .request(kvs, 2s)
                   .then(
                     [prom]() mutable {
                       prom.respond(http::status::no_content);
                     },
                     [prom](const caf::error& what) mutable {
                       prom.respond(http::status::internal_server_error, what);
                     });
               })
        .route("/status", http::method::get,
               [kvs](http::responder& res) {
                 res.respond(http::status::no_content);
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
