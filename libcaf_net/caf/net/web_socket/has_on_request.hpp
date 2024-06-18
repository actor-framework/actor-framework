// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/dsl/generic_config.hpp"
#include "caf/net/dsl/server_factory_base.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/web_socket/acceptor.hpp"
#include "caf/net/web_socket/frame.hpp"
#include "caf/net/web_socket/server_factory.hpp"

#include "caf/async/spsc_buffer.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/ws_conn_acceptor.hpp"
#include "caf/intrusive_ptr.hpp"

#include <type_traits>

namespace caf::net::web_socket {

/// DSL entry point for creating a server.
class CAF_NET_EXPORT has_on_request
  : public dsl::server_factory_base<has_on_request> {
public:
  template <class Token, class... Args>
  has_on_request(Token token, const dsl::generic_config_value& from,
                 Args&&... args) {
    config_ = sfb::make_config(from.mpx);
    sfb::upcast(*config_).assign(from, token, std::forward<Args>(args)...);
  }

  ~has_on_request() override;

  /// Adds the handler for accepting or rejecting incoming requests.
  template <class OnRequest>
  auto on_request(OnRequest on_request) {
    // Type checking.
    using fn_trait = detail::get_callable_trait_t<OnRequest>;
    static_assert(fn_trait::num_args == 1,
                  "on_request must take exactly one argument");
    using arg_types = typename fn_trait::arg_types;
    using arg1_t = detail::tl_at_t<arg_types, 0>;
    using acceptor_t = std::decay_t<arg1_t>;
    static_assert(is_acceptor_v<acceptor_t>,
                  "on_request must take an acceptor as its argument");
    static_assert(std::is_same_v<arg1_t, acceptor_t&>,
                  "on_request must take the acceptor as mutable reference");
    // Wrap the callback and return the factory object.
    using impl_t = detail::ws_conn_acceptor_t<OnRequest, acceptor_t>;
    using factory_t = server_factory_t<acceptor_t>;
    using accept_event_t = typename impl_t::accept_event;
    auto [pull, push] = async::make_spsc_buffer_resource<accept_event_t>();
    auto wsa = make_counted<impl_t>(std::move(on_request), std::move(push));
    return factory_t{std::move(config_), std::move(wsa), std::move(pull)};
  }

protected:
  dsl::server_config_value& base_config() override;

private:
  using sfb = web_socket::server_factory_base;

  sfb::config_impl* config_ = nullptr;
};

} // namespace caf::net::web_socket
