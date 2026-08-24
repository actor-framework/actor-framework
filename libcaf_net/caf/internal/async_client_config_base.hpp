// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/ssl/context.hpp"

#include "caf/actor_system.hpp"
#include "caf/callback.hpp"
#include "caf/detail/connector.hpp"
#include "caf/ref_counted.hpp"
#include "caf/uri.hpp"

#include <memory>
#include <string_view>
#include <utility>

namespace caf::internal {

class async_client_config_base : public ref_counted {
public:
  explicit async_client_config_base(actor_system& owner) noexcept
    : sys(&owner) {
    // nop
  }

  struct scheme_and_port {
    std::string_view scheme;
    uint16_t port;
  };

  /// Returns the accepted URI schemes and their default ports as
  /// `{non-SSL, SSL}` pair.
  virtual std::pair<scheme_and_port, scheme_and_port> schemes() const = 0;

  actor_system* sys;

  uri endpoint;

  size_t max_retry_count = 0;

  timespan retry_delay = timespan{1'000'000};

  timespan connection_timeout = infinite;

  unique_callback_ptr<expected<net::ssl::context>()> context_factory;

  std::unique_ptr<detail::connector> connector;

  error err;
};

using async_client_config_base_ptr = intrusive_ptr<async_client_config_base>;
using const_async_client_config_base_ptr
  = intrusive_ptr<const async_client_config_base>;

} // namespace caf::internal
