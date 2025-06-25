// Simple WebSocket server that sends everything it receives back to the sender.

#include "caf/net/fwd.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/ssl/tcp_acceptor.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/web_socket/lower_layer.hpp"
#include "caf/net/web_socket/server.hpp"
#include "caf/net/web_socket/upper_layer.hpp"

#include "caf/action.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/detail/connection_factory.hpp"
#include "caf/internal/accept_handler.hpp"
#include "caf/internal/make_transport.hpp"
#include "caf/log/net.hpp"

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

  void abort([[maybe_unused]] const caf::error& reason) override {
    caf::log::net::error("{}", reason);
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

template <class UpperLayerPtr>
auto make_transport(caf::net::stream_socket fd, UpperLayerPtr upper_layer) {
  return caf::net::octet_stream::transport::make(std::move(fd),
                                                 std::move(upper_layer));
}

template <class UpperLayerPtr>
auto make_transport(caf::net::ssl::connection conn, UpperLayerPtr upper_layer) {
  return caf::net::ssl::transport::make(std::move(conn),
                                        std::move(upper_layer));
}

template <class Transport>
class conn_factory : public caf::detail::connection_factory<
                       typename Transport::connection_handle> {
public:
  using connection_handle = typename Transport::connection_handle;

  caf::net::socket_manager_ptr make(caf::net::multiplexer* mpx,
                                    connection_handle conn) override {
    auto app = web_socket_app::make();
    auto server = caf::net::web_socket::server::make(std::move(app));
    auto transport = make_transport(std::move(conn), std::move(server));
    return caf::net::socket_manager::make(mpx, std::move(transport));
  }
};

/// Accepts incoming clients with an Acceptor and handles them via a connection
/// factory.
template <class Acceptor>
class accept_handler : public caf::net::socket_event_layer {
public:
  // -- member types -----------------------------------------------------------

  using socket_type = caf::net::socket;

  using transport_type = typename Acceptor::transport_type;

  using connection_handle = typename transport_type::connection_handle;

  using factory_type = caf::detail::connection_factory<connection_handle>;

  using factory_ptr = caf::detail::connection_factory_ptr<connection_handle>;

  // -- constructors, destructors, and assignment operators --------------------

  accept_handler(Acceptor acc, factory_ptr fptr, size_t max_connections)
    : acc_(std::move(acc)),
      factory_(std::move(fptr)),
      max_connections_(max_connections) {
    CAF_ASSERT(max_connections_ > 0);
  }

  ~accept_handler() override {
    on_conn_close_.dispose();
    if (valid(acc_))
      close(acc_);
    if (monitor_callback_)
      monitor_callback_.dispose();
  }

  // -- factories --------------------------------------------------------------

  static std::unique_ptr<accept_handler> make(Acceptor acc, factory_ptr fptr,
                                              size_t max_connections) {
    return std::make_unique<accept_handler>(std::move(acc), std::move(fptr),
                                            max_connections);
  }

  // -- implementation of socket_event_layer -----------------------------------

  caf::error start(caf::net::socket_manager* owner) override {
    self_ref_ = owner->as_disposable();
    owner_ = owner;
    if (auto err = factory_->start(owner)) {
      return err;
    }
    on_conn_close_ = caf::make_action([this] { connection_closed(); });
    owner->register_reading();
    return {};
  }

  caf::net::socket handle() const override {
    return acc_.fd();
  }

  void handle_read_event() override {
    CAF_ASSERT(owner_ != nullptr);
    if (open_connections_.size() == max_connections_) {
      owner_->deregister_reading();
    } else if (auto conn = accept(acc_)) {
      auto child = factory_->make(owner_->mpx_ptr(), std::move(*conn));
      if (!child) {
        on_conn_close_.dispose();
        owner_->shutdown();
        return;
      }
      open_connections_.push_back(child->as_disposable());
      if (open_connections_.size() == max_connections_)
        owner_->deregister_reading();
      child->add_cleanup_listener(on_conn_close_);
      std::ignore = child->start();
    } else if (conn.error() == caf::sec::unavailable_or_would_block) {
      // Encountered a "soft" error: simply try again later.
    } else {
      // Encountered a "hard" error: stop.
      abort(conn.error());
      owner_->deregister_reading();
    }
  }

  void handle_write_event() override {
    owner_->deregister_writing();
  }

  void abort(const caf::error& reason) override {
    factory_->abort(reason);
    on_conn_close_.dispose();
    self_ref_ = nullptr;
    for (auto& hdl : open_connections_)
      hdl.dispose();
    open_connections_.clear();
  }

  void self_ref(caf::disposable ref) {
    self_ref_ = std::move(ref);
  }

private:
  void connection_closed() {
    auto& conns = open_connections_;
    auto new_end = std::remove_if(conns.begin(), conns.end(),
                                  [](auto& ptr) { return ptr.disposed(); });
    if (new_end == conns.end())
      return;
    if (open_connections_.size() == max_connections_)
      owner_->register_reading();
    conns.erase(new_end, conns.end());
  }

  Acceptor acc_;

  factory_ptr factory_;

  size_t max_connections_;

  std::vector<caf::disposable> open_connections_;

  caf::net::socket_manager* owner_ = nullptr;

  caf::action on_conn_close_;

  /// Type-erased handle to the @ref socket_manager. This reference is important
  /// to keep the acceptor alive while the manager is not registered for writing
  /// or reading.
  caf::disposable self_ref_;

  /// An action for stopping this handler if an observed actor terminates.
  caf::action monitor_callback_;

  /// List of actors that we add monitors to in `start`.
  std::vector<caf::strong_actor_ptr> monitored_actors_;
};

namespace {

std::atomic<bool> shutdown_flag;

} // namespace

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
  auto fd = caf::net::make_tcp_accept_socket(port);
  if (!fd) {
    std::cerr << "*** failed to open port: " << to_string(fd.error()) << '\n';
    return EXIT_FAILURE;
  }
  auto ctx = ssl::context::enable(key_file && cert_file)
               .and_then(ssl::emplace_server(ssl::tls::v1_2))
               .and_then(ssl::use_private_key_file(key_file, pem))
               .and_then(ssl::use_certificate_file(cert_file, pem));
  auto impl = std::unique_ptr<caf::net::socket_event_layer>{};
  if (ctx) {
    using factory_t = conn_factory<ssl::transport>;
    using impl_t = accept_handler<ssl::tcp_acceptor>;
    auto acceptor = ssl::tcp_acceptor{std::move(*fd), std::move(*ctx)};
    impl = impl_t::make(std::move(acceptor), std::make_unique<factory_t>(),
                        max_connections);
  } else if (!ctx.error()) { // SSL disabled.
    using factory_t = conn_factory<caf::net::octet_stream::transport>;
    using impl_t = accept_handler<caf::net::tcp_accept_socket>;
    impl = impl_t::make(std::move(*fd), std::make_unique<factory_t>(),
                        max_connections);
  } else {
    std::cerr << "*** failed to initialize SSL: " << to_string(ctx.error())
              << '\n';
    return EXIT_FAILURE;
  }
  auto* mpx = sys.network_manager().mpx_ptr();
  auto mgr = caf::net::socket_manager::make(mpx, std::move(impl));
  mgr->add_cleanup_listener(caf::make_single_shot_action([] { //
    shutdown_flag = true;
  }));
  if (!mpx->start(mgr))
    return EXIT_FAILURE;
  // Wait until the server shuts down.
  while (!shutdown_flag.load()) {
    std::this_thread::sleep_for(1s);
  }
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
