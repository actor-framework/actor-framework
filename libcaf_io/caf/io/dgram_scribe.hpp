/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_DGRAM_SCRIBE_HPP
#define CAF_IO_DGRAM_SCRIBE_HPP

#include <vector>

#include "caf/message.hpp"

#include "caf/io/broker_servant.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/dgram_scribe_handle.hpp"
#include "caf/io/network/dgram_acceptor_manager.hpp"

namespace caf {
namespace io {

using dgram_scribe_base = broker_servant<network::dgram_communicator_manager,
                                         dgram_scribe_handle,
                                         new_datagram_msg>;

/// Manages writing to a datagram sink.
/// @ingroup Broker
class dgram_scribe : public dgram_scribe_base {
public:
  dgram_scribe(abstract_broker* parent, dgram_scribe_handle hdl);

  ~dgram_scribe();

  /// Configure buffer size for next accepted datagram.
  virtual void configure_datagram_size(size_t buf_size) = 0;

  /// Enables or disables write notifications.
  virtual void ack_writes(bool enable) = 0;

  /// Returns the current output buffer.
  virtual std::vector<char>& wr_buf() = 0;

  /// Returns the current input buffer.
  virtual std::vector<char>& rd_buf() = 0;

  /// Flushes the output buffer, i.e., sends the
  /// content of the buffer via the network.
  virtual void flush() = 0;

  virtual uint16_t local_port() const = 0;

  void io_failure(execution_unit* ctx, network::operation op) override;

  bool consume(execution_unit*, const void*, size_t) override;

  void datagram_sent(execution_unit*, size_t) override;

protected:
  message detach_message() override;
};

} // namespace io
} // namespace caf

#endif // CAF_IO_DGRAM_SCRIBE_HPP
