// Simple HTTP client that prints the response

#include "caf/net/http/client.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/octet_stream/transport.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/detail/latch.hpp"
#include "caf/ipv4_address.hpp"

#include <iostream>
#include <memory>
#include <utility>

using namespace std::literals;

namespace http = caf::net::http;
using namespace caf;

// -- configuration setup ------------------------------------------------------
struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
      .add<http::method>("method,m", "HTTP method to use")
      .add<uri>("resource,r", "Requested resource (URL)")
      .add<std::string>("payload,p", "Optional payload to send");
  }
};

constexpr auto default_method = http::method::get;

// -- http client sending get request and awaiting response --------------------
class http_app : public http::upper_layer::client {
public:
  static auto make(http::method method, std::string resource,
                   std::string payload) {
    return std::make_unique<http_app>(method, std::move(resource),
                                      std::move(payload));
  }

  http_app(http::method method, std::string resource, std::string payload)
    : method_{method},
      resource_{std::move(resource)},
      payload_{std::move(payload)} {
    latch_ = std::make_shared<detail::latch>(2);
  }

  ~http_app() {
    latch_->count_down();
  }

  virtual void prepare_send() override {
    // nop
  }

  [[nodiscard]] virtual bool done_sending() override {
    return true;
  }

  virtual void abort(const caf::error& reason) override {
    printf("%s\n", to_string(reason).c_str());
  }

  caf::error start(http::lower_layer::client* ll) override {
    std::cout << "HTTP Client started" << std::endl;
    down = ll;
    // Send request.
    down->begin_header(method_, resource_);
    if (!payload_.empty())
      down->add_header_field("Content-Length", std::to_string(payload_.size()));
    down->end_header();
    if (!payload_.empty())
      down->send_payload(as_bytes(make_span(payload_)));
    // Await response.
    down->request_messages();
    return caf::none;
  }

  ptrdiff_t consume(const http::response_header& hdr,
                    caf::const_byte_span payload) override {
    std::cout
      << "Got response: " << hdr.status() << ' ' << hdr.status_text()
      << std::endl
      // << std::string_view{reinterpret_cast<const char*>(payload.data()),
      //                     payload.size()}
      << std::endl;
    return static_cast<ptrdiff_t>(payload.size());
  }

  std::shared_ptr<detail::latch> latch() const noexcept {
    return latch_;
  }

private:
  http::lower_layer::client* down = nullptr;
  http::method method_;
  std::string resource_;
  std::string payload_;
  std::shared_ptr<detail::latch> latch_;
};

int caf_main(caf::actor_system& sys, const config& cfg) {
  auto maybe_resource = caf::get_as<uri>(cfg, "resource");
  if (!maybe_resource) {
    std::cerr << "*** missing mandatory option --resource" << std::endl;
    return EXIT_FAILURE;
  }
  auto resource = std::move(*maybe_resource);
  if (resource.scheme() != "http") {
    std::cerr << "*** only HTTP is supported at the moment" << std::endl;
    return EXIT_FAILURE;
  }
  auto method = caf::get_or(cfg, "method", default_method);
  auto payload = caf::get_or(cfg, "payload", ""sv);
  auto sock = caf::net::make_connected_tcp_stream_socket(resource.authority());
  if (!sock) {
    std::cerr << "*** failed to connect to " << to_string(resource.authority())
              << std::endl;
    return EXIT_FAILURE;
  }
  auto& mpx = sys.network_manager().mpx();
  auto app = http_app::make(method, resource.path_query_fragment(), payload);
  auto latch = app->latch();
  auto http_client = http::client::make(std::move(app));
  auto transport
    = caf::net::octet_stream::transport::make(*sock, std::move(http_client));
  mpx.start(caf::net::socket_manager::make(&mpx, std::move(transport)));
  latch->count_down_and_wait();
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
