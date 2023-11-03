// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/middleman.hpp"

#include "caf/net/http/with.hpp"
#include "caf/net/prometheus.hpp"
#include "caf/net/ssl/startup.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/net/this_host.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/expected.hpp"
#include "caf/raise_error.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/thread_owner.hpp"
#include "caf/uri.hpp"

namespace caf::net {

namespace {

struct prom_config {
  uint16_t port;
  std::string address = "0.0.0.0";
  bool reuse_address = true;
};

template <class Inspector>
bool inspect(Inspector& f, prom_config& x) {
  return f.object(x).fields(
    f.field("port", x.port), f.field("address", x.address).fallback("0.0.0.0"),
    f.field("reuse-address", x.reuse_address).fallback(true));
}

void launch_prom_server(actor_system& sys, const prom_config& cfg) {
  auto server = http::with(sys)
                  .accept(cfg.port, cfg.address)
                  .reuse_address(cfg.reuse_address)
                  .route("/metrics", prometheus::scraper(sys))
                  .start();
  if (!server)
    CAF_LOG_WARNING("failed to start Prometheus server: " << server.error());
}

void launch_background_tasks(actor_system& sys) {
  auto& cfg = sys.config();
  if (auto pcfg = get_as<prom_config>(cfg, "caf.middleman.prometheus-http")) {
    launch_prom_server(sys, *pcfg);
  }
}

} // namespace

void middleman::init_global_meta_objects() {
  // nop
}

actor_system::global_state_guard middleman::init_host_system() {
  this_host::startup();
  ssl::startup();
  return actor_system::global_state_guard{+[] {
    ssl::cleanup();
    this_host::cleanup();
  }};
}

middleman::middleman(actor_system& sys)
  : sys_(sys), mpx_(multiplexer::make(this)) {
  // nop
}

middleman::~middleman() {
  // nop
}

void middleman::start() {
  auto fn = [this] {
    mpx_->set_thread_id();
    launch_background_tasks(sys_);
    mpx_->run();
  };
  mpx_thread_ = sys_.launch_thread("caf.net.mpx", thread_owner::system, fn);
}

void middleman::stop() {
  mpx_->shutdown();
  if (mpx_thread_.joinable())
    mpx_thread_.join();
  else
    mpx_->run();
}

void middleman::init(actor_system_config&) {
  if (auto err = mpx_->init()) {
    CAF_LOG_ERROR("mpx_->init() failed: " << err);
    CAF_RAISE_ERROR("mpx_->init() failed");
  }
}

middleman::module::id_t middleman::id() const {
  return module::network_manager;
}

void* middleman::subtype_ptr() {
  return this;
}

actor_system::module* middleman::make(actor_system& sys, detail::type_list<>) {
  return new middleman(sys);
}

void middleman::add_module_options(actor_system_config& cfg) {
  config_option_adder{cfg.custom_options(), "caf.middleman.prometheus-http"}
    .add<uint16_t>("port", "listening port for incoming scrapes")
    .add<std::string>("address", "bind address for the HTTP server socket");
}

} // namespace caf::net
