// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/middleman.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/detail/set_thread_name.hpp"
#include "caf/expected.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/middleman_backend.hpp"
#include "caf/raise_error.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"
#include "caf/uri.hpp"

namespace caf::net {

void middleman::init_global_meta_objects() {
  caf::init_global_meta_objects<id_block::net_module>();
}

middleman::middleman(actor_system& sys) : sys_(sys), mpx_(this) {
  // nop
}

middleman::~middleman() {
  // nop
}

void middleman::start() {
  if (!get_or(config(), "caf.middleman.manual-multiplexing", false)) {
    mpx_thread_ = std::thread{[this] {
      CAF_SET_LOGGER_SYS(&sys_);
      detail::set_thread_name("caf.net.mpx");
      sys_.thread_started();
      mpx_.set_thread_id();
      mpx_.run();
      sys_.thread_terminates();
    }};
  } else {
    mpx_.set_thread_id();
  }
}

void middleman::stop() {
  for (const auto& backend : backends_)
    backend->stop();
  mpx_.shutdown();
  if (mpx_thread_.joinable())
    mpx_thread_.join();
  else
    mpx_.run();
}

void middleman::init(actor_system_config& cfg) {
  if (auto err = mpx_.init()) {
    CAF_LOG_ERROR("mpx_.init() failed: " << err);
    CAF_RAISE_ERROR("mpx_.init() failed");
  }
  if (auto node_uri = get_if<uri>(&cfg, "caf.middleman.this-node")) {
    auto this_node = make_node_id(std::move(*node_uri));
    sys_.node_.swap(this_node);
  } else {
    // CAF_RAISE_ERROR("no valid entry for caf.middleman.this-node found");
  }
  for (auto& backend : backends_)
    if (auto err = backend->init()) {
      CAF_LOG_ERROR("failed to initialize backend: " << err);
      CAF_RAISE_ERROR("failed to initialize backend");
    }
}

middleman::module::id_t middleman::id() const {
  return module::network_manager;
}

void* middleman::subtype_ptr() {
  return this;
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
}

expected<endpoint_manager_ptr> middleman::connect(const uri& locator) {
  if (auto ptr = backend(locator.scheme()))
    return ptr->get_or_connect(locator);
  else
    return basp::ec::invalid_scheme;
}

void middleman::resolve(const uri& locator, const actor& listener) {
  auto ptr = backend(locator.scheme());
  if (ptr != nullptr)
    ptr->resolve(locator, listener);
  else
    anon_send(listener, error{basp::ec::invalid_scheme});
}

middleman_backend* middleman::backend(string_view scheme) const noexcept {
  auto predicate = [&](const middleman_backend_ptr& ptr) {
    return ptr->id() == scheme;
  };
  auto i = std::find_if(backends_.begin(), backends_.end(), predicate);
  if (i != backends_.end())
    return i->get();
  return nullptr;
}

expected<uint16_t> middleman::port(string_view scheme) const {
  if (auto ptr = backend(scheme))
    return ptr->port();
  else
    return basp::ec::invalid_scheme;
}

} // namespace caf::net
