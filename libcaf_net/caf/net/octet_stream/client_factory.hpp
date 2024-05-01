// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/socket_manager.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/flow_bridge_initializer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/observable.hpp"

#include <type_traits>

namespace caf::net::octet_stream {

/// Factory for the `with(...).connect(...).start(...)` DSL.
class CAF_NET_EXPORT client_factory
  : public dsl::client_factory_base<client_factory> {
public:
  template <class Token, class... Args>
  client_factory(Token token, const dsl::generic_config_value& from,
                 Args&&... args) {
    init_config(from.mpx).assign(from, token, std::forward<Args>(args)...);
  }

  ~client_factory() override;

  /// Overrides the default buffer size for reading from the network.
  client_factory&& read_buffer_size(uint32_t new_value) &&;

  /// Overrides the default buffer size for writing to the network.
  client_factory&& write_buffer_size(uint32_t new_value) &&;

  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) {
    static_assert(std::is_invocable_v<OnStart, pull_t, push_t>);
    // Create socket-to-application and application-to-socket buffers.
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<std::byte>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<std::byte>();
    // Wrap the trait and the buffers that belong to the socket.
    auto res = base_config().visit(
      [this, pull = a2s_pull, push = s2a_push](auto& data) {
        return this->do_start(data, std::move(pull), std::move(push));
      });
    if (res) {
      on_start(std::move(s2a_pull), std::move(a2s_push));
    }
    return res;
  }

protected:
  dsl::client_config_value& base_config() override;

private:
  class config_impl;

  using pull_t = async::consumer_resource<std::byte>;

  using push_t = async::producer_resource<std::byte>;

  dsl::client_config_value& init_config(multiplexer* mpx);

  expected<disposable> do_start(dsl::client_config::lazy& data,
                                dsl::server_address& addr, pull_t pull,
                                push_t push);

  expected<disposable> do_start(dsl::client_config::lazy&, const uri&,
                                pull_t pull, push_t push);

  expected<disposable> do_start(dsl::client_config::lazy& data, pull_t pull,
                                push_t push);

  expected<disposable> do_start(dsl::client_config::socket& data, pull_t pull,
                                push_t push);

  expected<disposable> do_start(dsl::client_config::conn& data, pull_t pull,
                                push_t push);

  expected<disposable> do_start(error& err, pull_t pull, push_t push);

  config_impl* config_ = nullptr;
};

} // namespace caf::net::octet_stream
