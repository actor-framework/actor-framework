// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/proxy_registry.hpp"

namespace caf::net {

/// Technology-specific backend for connecting to and managing peer
/// connections.
/// @relates middleman
class CAF_NET_EXPORT middleman_backend : public proxy_registry::backend {
public:
  // -- constructors, destructors, and assignment operators --------------------

  middleman_backend(std::string id);

  virtual ~middleman_backend();

  // -- interface functions ----------------------------------------------------

  /// Initializes the backend.
  virtual error init() = 0;

  /// @returns The endpoint manager for `peer` on success, `nullptr` otherwise.
  virtual endpoint_manager_ptr peer(const node_id& id) = 0;

  /// Establishes a connection to a remote node.
  virtual expected<endpoint_manager_ptr> get_or_connect(const uri& locator) = 0;

  /// Resolves a path to a remote actor.
  virtual void resolve(const uri& locator, const actor& listener) = 0;

  virtual void stop() = 0;

  // -- properties -------------------------------------------------------------

  const std::string& id() const noexcept {
    return id_;
  }

  virtual uint16_t port() const = 0;

private:
  /// Stores the technology-specific identifier.
  std::string id_;
};

/// @relates middleman_backend
using middleman_backend_ptr = std::unique_ptr<middleman_backend>;

} // namespace caf::net
