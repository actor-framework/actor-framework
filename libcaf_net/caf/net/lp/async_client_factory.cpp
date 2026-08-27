// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/lp/async_client_factory.hpp"

#include "caf/net/lp/framing.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/actor_system.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/actor_system_access.hpp"
#include "caf/internal/connect_job.hpp"
#include "caf/internal/lp_flow_bridge.hpp"
#include "caf/internal/make_transport.hpp"
#include "caf/make_counted.hpp"
#include "caf/scheduler.hpp"
#include "caf/sec.hpp"

#include <utility>

namespace caf::net::lp {

class async_client_config : public internal::async_client_config_base {
public:
  using super = internal::async_client_config_base;

  using super::super;

  std::pair<scheme_and_port, scheme_and_port> schemes() const override {
    return {{"tcp", 0}, {"ssl", 0}};
  }

  size_field_type size_field = size_field_type::u4;

  size_t max_message_size = defaults::net::lp_max_message_size;
};

void intrusive_ptr_add_ref(const async_client_config* ptr) noexcept {
  ptr->ref();
}

void intrusive_ptr_release(const async_client_config* ptr) noexcept {
  ptr->deref();
}

namespace {

template <class Connection>
void start_connection(const_async_client_config_ptr config,
                      async::consumer_resource<chunk> pull,
                      async::producer_resource<chunk> push, Connection conn) {
  auto bridge = internal::make_lp_flow_bridge(pull, push);
  auto impl = framing::make(std::move(bridge), config->size_field,
                            config->max_message_size);
  auto transport = internal::make_transport(std::move(conn), std::move(impl));
  transport->active_policy().connect();
  auto* mpx = config->sys->network_manager().mpx_ptr();
  auto mgr = socket_manager::make(mpx, std::move(transport));
  if (!mpx->start(mgr)) {
    auto what = make_error(sec::runtime_error,
                           "failed to start socket manager");
    push.abort(what);
    pull.cancel();
    return;
  }
}

/// Establishes the outbound TCP connection on the async worker thread pool
/// and then hands off to the caf.net multiplexer.
class connect_job : public internal::connect_job {
public:
  connect_job(const_async_client_config_ptr config,
              async::consumer_resource<chunk> pull,
              async::producer_resource<chunk> push)
    : internal::connect_job(config),
      config_(std::move(config)),
      pull_(std::move(pull)),
      push_(std::move(push)) {
    // nop
  }

protected:
  bool canceled() const noexcept override {
    return push_.canceled();
  }

  void fail(error reason) override {
    push_.abort(reason);
    pull_.cancel();
  }

private:
  void handover(net::stream_socket fd) override {
    start_connection(config_, std::move(pull_), std::move(push_),
                     std::move(fd));
  }

  void handover(net::ssl::connection conn) override {
    start_connection(config_, std::move(pull_), std::move(push_),
                     std::move(conn));
  }

  const_async_client_config_ptr config_;

  async::consumer_resource<chunk> pull_;

  async::producer_resource<chunk> push_;
};

void schedule_connect(const_async_client_config_ptr config,
                      async::consumer_resource<chunk> pull,
                      async::producer_resource<chunk> push) {
  if (config->err.valid()) {
    push.abort(config->err);
    pull.cancel();
    return;
  }
  auto job = make_counted<connect_job>(config, std::move(pull),
                                       std::move(push));
  auto* sys = detail::actor_system_access{*config->sys}.impl();
  sys->async_workers().schedule(std::move(job), 0);
}

} // namespace

async_client_factory_builder::async_client_factory_builder(actor_system& sys,
                                                           uri endpoint)
  : config_(make_counted<async_client_config>(sys)) {
  config_->endpoint = std::move(endpoint);
}

async_client_factory_builder::async_client_factory_builder(
  actor_system& sys, expected<uri> endpoint)
  : config_(make_counted<async_client_config>(sys)) {
  if (endpoint) {
    config_->endpoint = std::move(*endpoint);
  } else {
    config_->err = std::move(endpoint.error());
  }
}

async_client_factory_builder::~async_client_factory_builder() noexcept {
  // nop
}

async_client_factory_builder&&
async_client_factory_builder::retry_delay(timespan value) && {
  config_->retry_delay = value;
  return std::move(*this);
}

async_client_factory_builder&&
async_client_factory_builder::connection_timeout(timespan value) && {
  config_->connection_timeout = value;
  return std::move(*this);
}

async_client_factory_builder&&
async_client_factory_builder::max_retry_count(size_t value) && {
  config_->max_retry_count = value;
  return std::move(*this);
}

async_client_factory_builder&&
async_client_factory_builder::size_field(size_field_type value) && {
  config_->size_field = value;
  return std::move(*this);
}

async_client_factory_builder&&
async_client_factory_builder::max_message_size(size_t value) && {
  config_->max_message_size = value;
  return std::move(*this);
}

async_client_factory_builder&& async_client_factory_builder::connector(
  std::unique_ptr<detail::connector>&& ptr) && {
  config_->connector = std::move(ptr);
  return std::move(*this);
}

async_client_factory async_client_factory_builder::factory() && {
  return async_client_factory{std::move(config_)};
}

void async_client_factory_builder::set_context_factory(
  unique_callback_ptr<expected<ssl::context>()> fn) {
  config_->context_factory = std::move(fn);
}

async_client_factory::~async_client_factory() noexcept {
  // nop
}

void async_client_factory::start_impl(pull_t pull, push_t push) const {
  schedule_connect(config_, std::move(pull), std::move(push));
}

} // namespace caf::net::lp
