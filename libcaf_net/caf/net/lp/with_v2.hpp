// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/lp/async_client_factory.hpp"

#include "caf/fwd.hpp"

namespace caf::net::lp {

/// Utility type for the fluent interface to asynchronous length-prefix
/// framing clients.
class with_async {
public:
  explicit with_async(actor_system& sys) : sys_(&sys) {
    // nop
  }

  /// Creates a new async client factory object for the given TCP `endpoint`.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns an async client factory initialized with the given parameters.
  [[nodiscard]] async_client_factory_builder connect(uri endpoint) const {
    return {*sys_, std::move(endpoint)};
  }

  /// Creates a new async client factory object for the given TCP `endpoint`.
  /// @param endpoint The endpoint of the TCP server to connect to.
  /// @returns an async client factory initialized with the given parameters.
  [[nodiscard]] async_client_factory_builder
  connect(expected<uri> endpoint) const {
    return {*sys_, std::move(endpoint)};
  }

private:
  actor_system* sys_;
};

/// Entrypoint into the fluent length-prefix framing DSL for asynchronous
/// operations.
class with_v2_t {
public:
  explicit with_v2_t(actor_system& sys) : sys_(&sys) {
    // nop
  }

  /// Entrypoint into the fluent length-prefix framing DSL for asynchronous
  /// operations.
  [[nodiscard]] with_async async() const {
    return with_async{*sys_};
  }

private:
  actor_system* sys_;
};

/// Creates a new entrypoint into the fluent length-prefix framing DSL for
/// asynchronous client connections. Unlike `with(...)`, this API pushes the
/// initial TCP connection attempt to the async workers thread pool instead of
/// blocking the calling thread while establishing the connection.
inline with_v2_t with_v2(actor_system& sys) {
  return with_v2_t{sys};
}

} // namespace caf::net::lp
