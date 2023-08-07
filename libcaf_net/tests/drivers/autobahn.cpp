// Simple WebSocket server that sends everything it receives back to the sender.

#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/web_socket/acceptor.hpp"
#include "caf/net/web_socket/framing.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/upper_layer.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"

#include <iostream>
#include <utility>

// -- constants ----------------------------------------------------------------

static constexpr uint16_t default_port = 8080;

static constexpr size_t default_max_connections = 128;

// -- configuration setup ------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<size_t>("max-connections,m", "limit for concurrent clients");
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

  caf::error accept(const caf::net::http::request_header& hdr) override {
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
  namespace ws = caf::net::web_socket;
  // Read the configuration.
  auto port = caf::get_or(cfg, "port", default_port);
  auto max_connections = caf::get_or(cfg, "max-connections",
                                     default_max_connections);

  auto app_layer = web_socket_app::make();
  auto app = app_layer.get();
  auto uut_layer
    = caf::net::web_socket::framing::make_server(std::move(app_layer));
  auto uut = uut_layer.get();

  auto transport
    = caf::net::octet_stream::transport::make(std::move(uut_layer));

  // transport->configure_read(caf::net::receive_policy::up_to(2048u));

  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
