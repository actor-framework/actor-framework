// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/network/manager.hpp"

#include "caf/detail/io_export.hpp"

#include <cstddef>

namespace caf::io::network {

/// A stream manager configures an IO stream and provides callbacks
/// for incoming data as well as for error handling.
class CAF_IO_EXPORT stream_manager : public manager {
public:
  ~stream_manager() override;

  /// Called by the underlying I/O device whenever it received data.
  /// @returns `true` if the manager accepts further reads, otherwise `false`.
  virtual bool consume(scheduler* ctx, const void* buf, size_t bsize) = 0;

  /// Called by the underlying I/O device whenever it sent data.
  virtual void
  data_transferred(scheduler* ctx, size_t num_bytes, size_t remaining_bytes)
    = 0;

  /// Get the port of the underlying I/O device.
  virtual uint16_t port() const = 0;

  /// Get the address of the underlying I/O device.
  virtual std::string addr() const = 0;
};

} // namespace caf::io::network
