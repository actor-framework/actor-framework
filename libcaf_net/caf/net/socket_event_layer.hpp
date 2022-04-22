// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/fwd.hpp"

namespace caf::net {

/// The lowest layer in a protocol stack. Called by a @ref socket_manager
/// directly.
class CAF_NET_EXPORT socket_event_layer {
public:
  virtual ~socket_event_layer();

  /// Encodes how to proceed after a read operation.
  enum class read_result {
    /// Indicates that a manager wants to read again later.
    again,
    /// Indicates that a manager wants to stop reading until explicitly resumed.
    stop,
    /// Indicates that a manager wants to write to the socket instead of reading
    /// from the socket.
    want_write,
    /// Indicates that the manager no longer reads from the socket.
    close,
    /// Indicates that the manager encountered a fatal error and stops both
    /// reading and writing.
    abort,
    /// Indicates that a manager is done with the socket and hands ownership to
    /// another manager.
    handover,
  };

  /// Encodes how to proceed after a write operation.
  enum class write_result {
    /// Indicates that a manager wants to read again later.
    again,
    /// Indicates that a manager wants to stop reading until explicitly resumed.
    stop,
    /// Indicates that a manager wants to read from the socket instead of
    /// writing to the socket.
    want_read,
    /// Indicates that the manager no longer writes to the socket.
    close,
    /// Indicates that the manager encountered a fatal error and stops both
    /// reading and writing.
    abort,
    /// Indicates that a manager is done with the socket and hands ownership to
    /// another manager.
    handover,
  };

  virtual error init(socket_manager* owner, const settings& cfg) = 0;

  virtual read_result handle_read_event() = 0;

  virtual read_result handle_buffered_data() = 0;

  virtual read_result handle_continue_reading() = 0;

  virtual write_result handle_write_event() = 0;

  virtual write_result handle_continue_writing() = 0;

  virtual void abort(const error& reason) = 0;
};

} // namespace caf::net
