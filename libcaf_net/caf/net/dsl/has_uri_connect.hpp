// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/make_counted.hpp"
#include "caf/net/dsl/base.hpp"
#include "caf/net/dsl/client_config.hpp"
#include "caf/net/dsl/has_connect.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/uri.hpp"

#include <cstdint>
#include <string>

namespace caf::net::dsl {

/// DSL entry point for creating a client from an URI.
template <class Base, class Subtype>
class has_uri_connect : public Base {
public:
  using trait_type = typename Base::trait_type;

  /// Creates a `connect_factory` object for the given TCP `endpoint`.
  ///
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  auto connect(const uri& endpoint) {
    auto& dref = static_cast<Subtype&>(*this);
    auto cfg = this->make_lazy_config(endpoint);
    return dref.lift(dref.with_context(std::move(cfg)));
  }

  /// Creates a `connect_factory` object for the given TCP `endpoint`.
  ///
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns a `connect_factory` object initialized with the given parameters.
  auto connect(expected<uri> endpoint) {
    if (endpoint)
      return connect(*endpoint);
    auto& dref = static_cast<Subtype&>(*this);
    auto cfg = this->make_fail_config(endpoint.error());
    return dref.lift(dref.with_context(std::move(cfg)));
  }
};

} // namespace caf::net::dsl
