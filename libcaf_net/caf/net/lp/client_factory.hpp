// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/checked_socket.hpp"
#include "caf/net/dsl/client_factory_base.hpp"
#include "caf/net/lp/config.hpp"
#include "caf/net/lp/frame.hpp"
#include "caf/net/lp/framing.hpp"
#include "caf/net/socket_manager.hpp"
#include "caf/net/ssl/connection.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/disposable.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <variant>

namespace caf::net::lp {

/// Factory for the `with(...).connect(...).start(...)` DSL.
class CAF_NET_EXPORT client_factory
  : public dsl::client_factory_base<client_factory> {
public:
  template <class Token, class... Args>
  client_factory(Token token, const dsl::generic_config_value& from,
                 Args&&... args) {
    init_config(from.mpx).assign(from, token, std::forward<Args>(args)...);
  }

  client_factory(client_factory&& other) noexcept;

  client_factory& operator=(client_factory&& other) noexcept;

  ~client_factory() override;

  /// Starts a connection with the length-prefixing protocol.
  template <class OnStart>
  [[nodiscard]] expected<disposable> start(OnStart on_start) {
    // using input_res_t = typename Trait::input_resource;
    // using output_res_t = typename Trait::output_resource;
    // static_assert(std::is_invocable_v<OnStart, input_res_t, output_res_t>);
    // return base_config().visit(
    //   [this, &on_start](auto& data) { return this->do_start(data, on_start);
    //   });

    // Create socket-to-application and application-to-socket buffers.
    auto [s2a_pull, s2a_push] = async::make_spsc_buffer_resource<frame>();
    auto [a2s_pull, a2s_push] = async::make_spsc_buffer_resource<frame>();
    // Wrap the trait and the buffers that belong to the socket.
    auto res = base_config().visit(
      [this, pull = a2s_pull, push = s2a_push](auto& data) mutable {
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

  using pull_t = async::consumer_resource<frame>;

  using push_t = async::producer_resource<frame>;

  dsl::client_config_value& init_config(multiplexer* mpx);

  expected<disposable> do_start(dsl::client_config::lazy& data, pull_t pull,
                                push_t push);

  expected<disposable> do_start(dsl::client_config::socket& data, pull_t pull,
                                push_t push);

  expected<disposable> do_start(dsl::client_config::conn& data, pull_t pull,
                                push_t push);

  expected<disposable> do_start(error& err, pull_t pull, push_t push);

  config_impl* config_ = nullptr;
};

} // namespace caf::net::lp
