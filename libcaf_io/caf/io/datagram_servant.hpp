// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <vector>

#include "caf/byte_buffer.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/io/broker_servant.hpp"
#include "caf/io/datagram_handle.hpp"
#include "caf/io/network/datagram_manager.hpp"
#include "caf/io/network/ip_endpoint.hpp"
#include "caf/io/network/receive_buffer.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/message.hpp"

namespace caf::io {

using datagram_servant_base = broker_servant<network::datagram_manager,
                                             datagram_handle, new_datagram_msg>;

/// Manages writing to a datagram sink.
/// @ingroup Broker
class CAF_IO_EXPORT datagram_servant : public datagram_servant_base {
public:
  datagram_servant(datagram_handle hdl);

  ~datagram_servant() override;

  /// Enables or disables write notifications.
  virtual void ack_writes(bool enable) = 0;

  /// Returns a new output buffer.
  virtual byte_buffer& wr_buf(datagram_handle) = 0;

  /// Enqueue a buffer to be sent as a datagram.
  virtual void enqueue_datagram(datagram_handle, byte_buffer) = 0;

  /// Returns the current input buffer.
  virtual network::receive_buffer& rd_buf() = 0;

  /// Flushes the output buffer, i.e., sends the
  /// content of the buffer via the network.
  virtual void flush() = 0;

  /// Returns the local port of associated socket.
  virtual uint16_t local_port() const = 0;

  /// Returns all the handles associated with this servant
  virtual std::vector<datagram_handle> hdls() const = 0;

  /// Adds a new remote endpoint identified by the `ip_endpoint` to
  /// the related manager.
  virtual void add_endpoint(const network::ip_endpoint& ep, datagram_handle hdl)
    = 0;

  virtual void remove_endpoint(datagram_handle hdl) = 0;

  bool consume(execution_unit*, datagram_handle hdl,
               network::receive_buffer& buf) override;

  void datagram_sent(execution_unit*, datagram_handle hdl, size_t,
                     byte_buffer buffer) override;

  virtual void detach_handles() = 0;

  using datagram_servant_base::new_endpoint;

  virtual void launch() = 0;

protected:
  message detach_message() override;
};

using datagram_servant_ptr = intrusive_ptr<datagram_servant>;

} // namespace caf::io

// Allows the `middleman_actor` to create an `datagram_servant` and then send it
// to the BASP broker.
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::io::datagram_servant_ptr)
