// Simple WebSocket server that sends everything it receives back to the sender.

#include "caf/net/middleman.hpp"
#include "caf/net/web_socket/upper_layer.hpp"
#include "caf/net/web_socket/with.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"

#include <iostream>
#include <utility>

using namespace std::literals;

namespace ssl = caf::net::ssl;
namespace ws = caf::net::web_socket;

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 8080;

static constexpr size_t default_max_connections = 128;

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<size_t>("max-connections,m", "limit for concurrent clients");
    opt_group{custom_options_, "tls"} //
      .add<std::string>("key-file,k", "path to the private key file")
      .add<std::string>("cert-file,c", "path to the certificate file");
  }
};

// -- synchronous web server implementation ------------------------------------

class web_socket_app : public caf::net::web_socket::upper_layer::server {
public:
  static auto make() {
    return std::make_unique<web_socket_app>();
  }

  caf::error start(caf::net::web_socket::lower_layer* ll) override {
    down = ll;
    down->request_messages();
    return caf::none;
  }

  caf::error accept(const caf::net::http::request_header&) override {
    return caf::none;
  }

  void prepare_send() override {
    // nop
  }

  bool done_sending() override {
    return true;
  }

  void abort(const caf::error& reason) override {
    CAF_LOG_ERROR(reason);
  }

  ptrdiff_t consume_text(std::string_view text) override {
    down->begin_text_message();
    auto& buffer = down->text_message_buffer();
    buffer.insert(buffer.end(), text.begin(), text.end());
    down->end_text_message();
    return static_cast<ptrdiff_t>(text.size());
  }

  ptrdiff_t consume_binary(caf::byte_span bytes) override {
    down->begin_binary_message();
    auto& buffer = down->binary_message_buffer();
    buffer.insert(buffer.end(), bytes.begin(), bytes.end());
    down->end_binary_message();
    return static_cast<ptrdiff_t>(bytes.size());
  }

  caf::net::web_socket::lower_layer* down = nullptr;
};

int caf_main(caf::actor_system& sys, const config& cfg) {
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto pem = ssl::format::pem;
  auto key_file = caf::get_as<std::string>(cfg, "tls.key-file");
  auto cert_file = caf::get_as<std::string>(cfg, "tls.cert-file");
  auto max_connections = caf::get_or(cfg, "max-connections",
                                     default_max_connections);
  if (!key_file != !cert_file) {
    std::cerr << "*** inconsistent TLS config: declare neither file or both\n";
    return EXIT_FAILURE;
  }
  // Open up a TCP port for incoming connections and start the server.
  auto server
    = ws::with(sys)
        // Optionally enable TLS.
        .context(ssl::context::enable(key_file && cert_file)
                   .and_then(ssl::emplace_server(ssl::tls::v1_2))
                   .and_then(ssl::use_private_key_file(key_file, pem))
                   .and_then(ssl::use_certificate_file(cert_file, pem)))
        // Bind to the user-defined port.
        .accept(port)
        // Limit how many clients may be connected at any given time.
        .max_connections(max_connections)
        // Add handler for incoming connections.
        .on_request([](ws::acceptor<>& acc) {
          // Ignore all header fields and accept the connection.
          acc.accept();
        })
        // Create instances of our app to handle incoming connections.
        .start([] { return web_socket_app::make(); });
  // Report any error to the user.
  if (!server) {
    std::cerr << "*** unable to run at port " << port << ": "
              << to_string(server.error()) << '\n';
    return EXIT_FAILURE;
  }
  // Wait until the server shuts down.
  while (server->valid()) {
    std::this_thread::sleep_for(1s);
  }
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
