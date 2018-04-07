/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include "caf/io/network/manager.hpp"

namespace caf {
namespace io {
namespace network {

/// An acceptor manager configures an acceptor and provides
/// callbacks for incoming connections as well as for error handling.
class acceptor_manager : public manager {
public:
  ~acceptor_manager() override;

  /// Called by the underlying I/O device to indicate that
  /// a new connection is awaiting acceptance.
  /// @returns `true` if the manager accepts further connections,
  ///          otherwise `false`.
  virtual bool new_connection() = 0;

  /// Get the port of the underlying I/O device.
  virtual uint16_t port() const = 0;
};

} // namespace network
} // namespace io
} // namespace caf

