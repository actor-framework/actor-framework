// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/network/manager.hpp"

#include "caf/detail/io_export.hpp"

namespace caf::io::network {

/// An acceptor manager configures an acceptor and provides
/// callbacks for incoming connections as well as for error handling.
class CAF_IO_EXPORT acceptor_manager : public manager {
public:
  ~acceptor_manager() override;

  /// Called by the underlying I/O device to indicate that
  /// a new connection is awaiting acceptance.
  /// @returns `true` if the manager accepts further connections,
  ///          otherwise `false`.
  virtual bool new_connection() = 0;

  /// Get the port of the underlying I/O device.
  virtual uint16_t port() const = 0;

  /// Get the port of the underlying I/O device.
  virtual std::string addr() const = 0;
};

} // namespace caf::io::network
