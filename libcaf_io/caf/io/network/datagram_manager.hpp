// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/datagram_handle.hpp"
#include "caf/io/network/manager.hpp"
#include "caf/io/network/receive_buffer.hpp"

#include "caf/detail/io_export.hpp"

namespace caf::io::network {

/// A datagram manager provides callbacks for outgoing
/// datagrams as well as for error handling.
class CAF_IO_EXPORT datagram_manager : public manager {
public:
  ~datagram_manager() override;

  /// Called by the underlying I/O device whenever it received data.
  /// @returns `true` if the manager accepts further reads, otherwise `false`.
  virtual bool
  consume(execution_unit*, datagram_handle hdl, receive_buffer& buf)
    = 0;

  /// Called by the underlying I/O device whenever it sent data.
  virtual void datagram_sent(execution_unit*, datagram_handle hdl, size_t,
                             byte_buffer buffer)
    = 0;

  /// Called by the underlying I/O device to indicate that a new remote
  /// endpoint has been detected, passing in the received datagram.
  /// @returns `true` if the manager accepts further endpoints,
  ///          otherwise `false`.
  virtual bool new_endpoint(receive_buffer& buf) = 0;

  /// Get the port of the underlying I/O device.
  virtual uint16_t port(datagram_handle) const = 0;

  /// Get the remote address of the underlying I/O device.
  virtual std::string addr(datagram_handle) const = 0;
};

} // namespace caf::io::network
