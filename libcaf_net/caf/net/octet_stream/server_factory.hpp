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

class CAF_NET_EXPORT server_factory
  : public dsl::server_factory_base<server_factory> {
public:
  template <class Token, class... Args>
  server_factory(Token token, const dsl::generic_config_value& from,
                 Args&&... args) {
    init_config(from.mpx).assign(from, token, std::forward<Args>(args)...);
  }

  ~server_factory() override;

  /// Monitors the actor handle @p hdl and stops the server if the monitored
  /// actor terminates.
  template <class ActorHandle>
  server_factory&& monitor(const ActorHandle& hdl) && {
    do_monitor(actor_cast<strong_actor_ptr>(hdl));
    return std::move(*this);
  }

  /// Overrides the default buffer size for reading from the network.
  server_factory&& read_buffer_size(uint32_t new_value) &&;

  /// Overrides the default buffer size for writing to the network.
  server_factory&& write_buffer_size(uint32_t new_value) &&;

  template <class OnStart>
  expected<disposable> start(OnStart on_start) {
    static_assert(std::is_invocable_v<OnStart, acceptor_resource<std::byte>>);
    using async::make_spsc_buffer_resource;
    auto [pull, push] = make_spsc_buffer_resource<accept_event<std::byte>>();
    auto res
      = base_config().visit([this, out = std::move(push)](auto& data) mutable {
          return this->do_start(data, std::move(out));
        });
    if (res) {
      on_start(std::move(pull));
    }
    return res;
  }

protected:
  dsl::server_config_value& base_config() override;

private:
  class config_impl;

  using push_t = async::producer_resource<accept_event<std::byte>>;

  dsl::server_config_value& init_config(multiplexer* mpx);

  void do_monitor(strong_actor_ptr ptr);

  expected<disposable> do_start(dsl::server_config::socket& data, push_t push);

  expected<disposable> do_start(dsl::server_config::lazy& data, push_t push);

  expected<disposable> do_start(error& err, push_t push);

  config_impl* config_ = nullptr;
};

} // namespace caf::net::octet_stream
