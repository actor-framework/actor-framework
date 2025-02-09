// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/middleman.hpp"

#include "caf/net/http/with.hpp"
#include "caf/net/prometheus.hpp"
#include "caf/net/ssl/startup.hpp"
#include "caf/net/this_host.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/expected.hpp"
#include "caf/log/net.hpp"
#include "caf/log/system.hpp"
#include "caf/raise_error.hpp"
#include "caf/thread_owner.hpp"

namespace caf::net {

namespace {

struct prom_tls_config {
  std::string key_file;
  std::string cert_file;
};

template <class Inspector>
bool inspect(Inspector& f, prom_tls_config& x) {
  return f.object(x).fields(f.field("key-file", x.key_file),
                            f.field("cert-file", x.cert_file));
}

std::optional<std::string> key_file(const std::optional<prom_tls_config>& cfg) {
  if (cfg)
    return cfg->key_file;
  return {};
}

std::optional<std::string>
cert_file(const std::optional<prom_tls_config>& cfg) {
  if (cfg)
    return cfg->cert_file;
  return {};
}

struct prom_config {
  uint16_t port;
  std::string address = "0.0.0.0";
  bool reuse_address = true;
  std::optional<prom_tls_config> tls;
};

template <class Inspector>
bool inspect(Inspector& f, prom_config& x) {
  return f.object(x).fields(
    f.field("port", x.port), //
    f.field("address", x.address).fallback("0.0.0.0"),
    f.field("reuse-address", x.reuse_address).fallback(true),
    f.field("tls", x.tls));
}

void launch_prom_server(actor_system& sys, const prom_config& cfg) {
  constexpr auto pem = ssl::format::pem;
  auto server
    = http::with(sys)
        .context(
          ssl::context::enable(cfg.tls.has_value())
            .and_then(ssl::emplace_server(ssl::tls::v1_2))
            .and_then(ssl::use_private_key_file(key_file(cfg.tls), pem))
            .and_then(ssl::use_certificate_file(cert_file(cfg.tls), pem)))
        .accept(cfg.port, cfg.address)
        .reuse_address(cfg.reuse_address)
        .route("/metrics", prometheus::scraper(sys))
        .start();
  if (!server)
    log::net::warning("failed to start Prometheus server: {}", server.error());
}

void launch_background_tasks(actor_system& sys) {
  auto& cfg = sys.config();
  if (auto pcfg = get_as<prom_config>(cfg, "caf.net.prometheus-http")) {
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
    log::system::error("failed to initialize multiplexer: {}", err);
    CAF_RAISE_ERROR("mpx_->init() failed");
  }
}

middleman::actor_system_module::id_t middleman::id() const {
  return actor_system_module::network_manager;
}

void* middleman::subtype_ptr() {
  return this;
}

void middleman::add_module_options(actor_system_config& cfg) {
  config_option_adder{cfg.custom_options(), "caf.net.prometheus-http"}
    .add<uint16_t>("port", "listening port for incoming scrapes")
    .add<std::string>("address", "bind address for the HTTP server socket")
    .add<bool>("reuse-address", "configure socket with SO_REUSEADDR");
  config_option_adder{cfg.custom_options(), "caf.net.prometheus-http.tls"}
    .add<std::string>("key-file", "path to the Promehteus private key file")
    .add<std::string>("cert-file", "path to the Promehteus private cert file");
}

actor_system_module* middleman::make(actor_system& sys) {
  return new middleman(sys);
}

void middleman::check_abi_compatibility(version::abi_token token) {
  if (static_cast<int>(token) != CAF_VERSION_MAJOR) {
    CAF_CRITICAL("CAF ABI token mismatch");
  }
}

} // namespace caf::net
