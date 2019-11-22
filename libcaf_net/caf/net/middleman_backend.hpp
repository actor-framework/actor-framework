/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <string>

#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"
#include "caf/proxy_registry.hpp"

namespace caf::net {

/// Technology-specific backend for connecting to and managing peer
/// connections.
/// @relates middleman
class middleman_backend : public proxy_registry::backend {
public:
  // -- constructors, destructors, and assignment operators --------------------

  middleman_backend(std::string id);

  virtual ~middleman_backend();

  // -- interface functions ----------------------------------------------------

  /// Initializes the backend.
  virtual error init() = 0;

  /// @returns The endpoint manager for `peer` on success, `nullptr` otherwise.
  virtual endpoint_manager_ptr peer(const node_id& id) = 0;

  /// Resolves a path to a remote actor.
  virtual void resolve(const uri& locator, const actor& listener) = 0;

  // -- properties -------------------------------------------------------------

  const std::string& id() const noexcept {
    return id_;
  }

private:
  /// Stores the technology-specific identifier.
  std::string id_;
};

/// @relates middleman_backend
using middleman_backend_ptr = std::unique_ptr<middleman_backend>;

} // namespace caf::net
