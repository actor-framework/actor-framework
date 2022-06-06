// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/middleman.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/expected.hpp"
#include "caf/net/prometheus/serve.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/raise_error.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"
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
  if (auto fd = make_tcp_accept_socket(cfg.port, cfg.address,
                                       cfg.reuse_address)) {
    CAF_LOG_INFO("start Prometheus server at port" << local_port(*fd));
    prometheus::serve(sys, std::move(*fd));
  } else {
    CAF_LOG_WARNING("failed to start Prometheus server: " << fd.error());
  }
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

middleman::middleman(actor_system& sys)
  : sys_(sys), mpx_(multiplexer::make(this)) {
  // nop
}

middleman::~middleman() {
  // nop
}

void middleman::start() {
  if (!get_or(config(), "caf.middleman.manual-multiplexing", false)) {
    mpx_thread_ = sys_.launch_thread("caf.net.mpx", [this] {
      mpx_->set_thread_id();
      launch_background_tasks(sys_);
      mpx_->run();
    });
  } else {
    mpx_->set_thread_id();
  }
}

void middleman::stop() {
  mpx_->shutdown();
  if (mpx_thread_.joinable())
    mpx_thread_.join();
  else
    mpx_->run();
}

void middleman::init(actor_system_config& cfg) {
  if (auto err = mpx_->init()) {
    CAF_LOG_ERROR("mpx_->init() failed: " << err);
    CAF_RAISE_ERROR("mpx_->init() failed");
  }
  if (auto node_uri = get_if<uri>(&cfg, "caf.middleman.this-node")) {
    auto this_node = make_node_id(std::move(*node_uri));
    sys_.node_.swap(this_node);
  } else {
    // CAF_RAISE_ERROR("no valid entry for caf.middleman.this-node found");
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
  config_option_adder{cfg.custom_options(), "caf.middleman"}
    .add<std::vector<std::string>>("app-identifiers",
                                   "valid application identifiers of this node")
    .add<uri>("this-node", "locator of this CAF node")
    .add<size_t>("max-consecutive-reads",
                 "max. number of consecutive reads per broker")
    .add<bool>("manual-multiplexing",
               "disables background activity of the multiplexer")
    .add<size_t>("workers", "number of deserialization workers")
    .add<timespan>("heartbeat-interval", "interval of heartbeat messages")
    .add<timespan>("connection-timeout",
                   "max. time between messages before declaring a node dead "
                   "(disabled if 0, ignored if heartbeats are disabled)")
    .add<std::string>("network-backend", "legacy option");
  config_option_adder{cfg.custom_options(), "caf.middleman.prometheus-http"}
    .add<uint16_t>("port", "listening port for incoming scrapes")
    .add<std::string>("address", "bind address for the HTTP server socket");
}

} // namespace caf::net
