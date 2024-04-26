// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/acceptor_resource.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/server_config.hpp"
#include "caf/net/dsl/server_factory_base.hpp"

#include "caf/detail/accept_handler.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/multicaster.hpp"
#include "caf/flow/observable.hpp"

#include <cstddef>
#include <type_traits>

namespace caf::net::octet_stream {

/// Configuration for `with(...).accept(...).start(...)` DSL.
class server_factory_config : public dsl::server_config_value {
public:
  using super = dsl::server_config_value;

  using super::super;

  /// Sets the default buffer size for reading from the network.
  uint32_t read_buffer_size = defaults::net::octet_stream_buffer_size;

  /// Sets the default buffer size for writing to the network.
  uint32_t write_buffer_size = defaults::net::octet_stream_buffer_size;

  /// Store actors that the server should monitor.
  std::vector<strong_actor_ptr> monitored_actors;

  template <class T, class... Args>
  static auto make(dsl::server_config_tag<T>,
                   const dsl::generic_config_value& from, Args&&... args) {
    return super::make_impl(std::in_place_type<server_factory_config>, from,
                            std::in_place_type<T>, std::forward<Args>(args)...);
  }
};

class CAF_NET_EXPORT server_factory
  : public dsl::server_factory_base<server_factory_config, server_factory> {
public:
  using super = dsl::server_factory_base<server_factory_config, server_factory>;

  using super::super;

  /// Monitors the actor handle @p hdl and stops the server if the monitored
  /// actor terminates.
  template <class ActorHandle>
  server_factory& monitor(const ActorHandle& hdl) {
    auto& cfg = super::config();
    auto ptr = actor_cast<strong_actor_ptr>(hdl);
    if (!ptr) {
      auto err = make_error(sec::logic_error,
                            "cannot monitor an invalid actor handle");
      cfg.fail(std::move(err));
      return *this;
    }
    cfg.monitored_actors.push_back(std::move(ptr));
    return *this;
  }

  /// Overrides the default buffer size for reading from the network.
  server_factory&& read_buffer_size(uint32_t new_value) && {
    config().read_buffer_size = new_value;
    return std::move(*this);
  }

  /// Overrides the default buffer size for writing to the network.
  server_factory&& write_buffer_size(uint32_t new_value) && {
    config().write_buffer_size = new_value;
    return std::move(*this);
  }

  template <class OnStart>
  expected<disposable> start(OnStart on_start) {
    static_assert(std::is_invocable_v<OnStart, acceptor_resource<std::byte>>);
    using async::make_spsc_buffer_resource;
    auto [pull, push] = make_spsc_buffer_resource<accept_event<std::byte>>();
    auto res
      = config().visit([this, out = std::move(push)](auto& data) mutable {
          return this->do_start(data, std::move(out));
        });
    if (res) {
      on_start(std::move(pull));
    }
    return res;
  }

private:
  using push_t = async::producer_resource<accept_event<std::byte>>;

  expected<disposable> do_start(dsl::server_config::socket& data, push_t push);

  expected<disposable> do_start(dsl::server_config::lazy& data, push_t push);

  expected<disposable> do_start(error& err, push_t push);
};

} // namespace caf::net::octet_stream
