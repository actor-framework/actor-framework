// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/octet_stream/flow_bridge.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/flow_bridge_initializer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/observable.hpp"

#include <functional>
#include <type_traits>

namespace caf::net::octet_stream {

/// Configuration for the `with(...).connect(...).start(...)` DSL.
class client_factory_config : public dsl::client_config_value {
public:
  using super = dsl::client_config_value;

  using super::super;

  /// Sets the default buffer size for reading from the network.
  uint32_t read_buffer_size = defaults::net::octet_stream_buffer_size;

  /// Sets the default buffer size for writing to the network.
  uint32_t write_buffer_size = defaults::net::octet_stream_buffer_size;

  template <class T, class... Args>
  static auto make(dsl::client_config_tag<T>,
                   const dsl::generic_config_value& from, Args&&... args) {
    return super::make_impl(std::in_place_type<client_factory_config>, from,
                            std::in_place_type<T>, std::forward<Args>(args)...);
  }
};

/// Factory for the `with(...).connect(...).start(...)` DSL.
class CAF_NET_EXPORT client_factory
  : public dsl::client_factory_base<client_factory_config, client_factory> {
public:
  using super = dsl::client_factory_base<client_factory_config, client_factory>;

  using super::super;

  using config_type = typename super::config_type;

  struct default_trait {
    using input_type = std::byte;

    using output_type = std::byte;

    using byte_observable = flow::observable<std::byte>;

    /// Maps from `output_type` to bytes for sending to the network.
    flow::observable<std::byte>
    map_outputs(flow::coordinator*, flow::observable<output_type> items) const {
      return items;
    }

    /// Maps the bytes received from the network to `input_type`.
    flow::observable<input_type>
    map_inputs(flow::coordinator*, flow::observable<std::byte> items) const {
      return items;
    }
  };

  /// Overrides the default buffer size for reading from the network.
  client_factory&& read_buffer_size(uint32_t new_value) && {
    config().read_buffer_size = new_value;
    return std::move(*this);
  }

  /// Overrides the default buffer size for writing to the network.
  client_factory&& write_buffer_size(uint32_t new_value) && {
    config().write_buffer_size = new_value;
    return std::move(*this);
  }

  template <class Trait, class OnStart>
  [[nodiscard]] expected<disposable> start(Trait trait, OnStart on_start) {
    using input_type = typename Trait::input_type;
    using output_type = typename Trait::output_type;
    using app_pull_t = async::consumer_resource<input_type>;
    using app_push_t = async::producer_resource<output_type>;
    static_assert(std::is_invocable_v<OnStart, app_pull_t, app_push_t>);
    // Create socket-to-application and application-to-socket buffers.
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<input_type>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<output_type>();
    // Wrap the trait and the buffers that belong to the socket.
    initializer_ = detail::make_flow_bridge_initializer(std::move(trait),
                                                        std::move(a2s_pull),
                                                        std::move(s2a_push));
    auto res = super::config().visit([this](auto& data) { //
      return this->do_start(data);
    });
    if (res) {
      on_start(std::move(s2a_pull), std::move(a2s_push));
    }
    return res;
  }

  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) {
    return start(default_trait{}, std::move(on_start));
  }

private:
  expected<disposable> do_start(dsl::client_config::lazy& data,
                                dsl::server_address& addr);

  expected<disposable> do_start(dsl::client_config::lazy&, const uri&);

  expected<disposable> do_start(dsl::client_config::lazy& data);

  expected<disposable> do_start(dsl::client_config::socket& data);

  expected<disposable> do_start(dsl::client_config::conn& data);

  expected<disposable> do_start(error& err);

  detail::flow_bridge_initializer_ptr initializer_;
};

} // namespace caf::net::octet_stream
